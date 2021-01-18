/*
 * This file is part of PowerDNS or weakforced.
 * Copyright -- PowerDNS.COM B.V. and its contributors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 3 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * In addition, for the avoidance of any doubt, permission is granted to
 * link this program with OpenSSL and to (re)distribute the binaries
 * produced as the result of such linking.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "config.h"
#include <stddef.h>
#include "wforce.hh"
#include "wforce_ns.hh"
#include "sstuff.hh"
#include "misc.hh"
#include <netinet/tcp.h>
#include <limits>
#include "dolog.hh"
#include <fstream>
#include "json11.hpp"
#include <unistd.h>
#include <sys/stat.h>
#include "sodcrypto.hh"
#include "perf-stats.hh"
#include "lock.hh"
#include "replication_sdb.hh"
#include "iputils.hh"
#include "ext/threadname.hh"
#include "wforce-replication.hh"
#include "wforce-prometheus.hh"

using std::atomic;
using std::thread;

struct SiblingQueueItem {
  std::string msg;
  ComboAddress remote;
  std::shared_ptr<Sibling> recv_sibling;
};

static std::mutex g_sibling_queue_mutex;
static std::queue<SiblingQueueItem> g_sibling_queue;
static std::condition_variable g_sibling_queue_cv;
static size_t max_sibling_queue_size = 5000;

GlobalStateHolder<vector<shared_ptr<Sibling>>> g_siblings;
unsigned int g_num_sibling_threads = WFORCE_NUM_SIBLING_THREADS;
SodiumNonce g_sodnonce;
std::mutex sod_mutx;

void encryptMsg(const std::string& msg, std::string& packet)
{
    std::lock_guard<std::mutex> lock(sod_mutx);
    packet=g_sodnonce.toString();
    packet+=sodEncryptSym(msg, g_key, g_sodnonce);
}

bool decryptMsg(const char* buf, size_t len, std::string& msg)
{
  SodiumNonce nonce;

  if (len < static_cast<size_t>(crypto_secretbox_NONCEBYTES)) {
    errlog("Could not decrypt replication operation: not enough bytes (%d) to hold nonce", len);
    return false;
  }
  memcpy((char*)&nonce, buf, crypto_secretbox_NONCEBYTES);
  string packet(buf + crypto_secretbox_NONCEBYTES, buf+len);
  try {
    msg=sodDecryptSym(packet, g_key, nonce);
  }
  catch (std::runtime_error& e) {
    errlog("Could not decrypt replication operation: %s", e.what());
    return false;
  }
  return true;
}

void replicateOperation(const ReplicationOperation& rep_op)
{
  auto siblings = g_siblings.getLocal();
  string msg = rep_op.serialize();
  string packet;

  encryptMsg(msg, packet);

  for(auto& s : *siblings) {
    s->queueMsg(packet);
  }
}

bool checkConnFromSibling(const ComboAddress& remote,
                          shared_ptr<Sibling>& recv_sibling)
{
  auto siblings = g_siblings.getLocal();

  for (auto &s : *siblings) {
    if (ComboAddress::addressOnlyEqual()(s->rem, remote) == true) {
      recv_sibling = s;
      break;
    }
  }

  if (recv_sibling == nullptr) {
    errlog("Message received from host (%s) that is not configured as a sibling", remote.toStringWithPort());
    return false;
  }
  return true;
}

void startReplicationWorkerThreads()
{
  // Tell prometheus how to get the recv queue size
  setPrometheusReplRecvQueueRetrieveFunc([]() -> int {
      std::lock_guard<std::mutex> lock(g_sibling_queue_mutex);
      return g_sibling_queue.size();
    });
  for (size_t i=0; i<g_num_sibling_threads; i++) {
    std::thread t([]() {
        thread_local bool init=false;
        if (!init) {
          setThreadName("wf/repl-worker");
          init = true;
        }
        while (true) {
          SiblingQueueItem sqi;
          {
            std::unique_lock<std::mutex> lock(g_sibling_queue_mutex);
            while (g_sibling_queue.size() == 0) {
              g_sibling_queue_cv.wait(lock);
            }
            sqi = std::move(g_sibling_queue.front());
            g_sibling_queue.pop();
          }
          ReplicationOperation rep_op;
          if (rep_op.unserialize(sqi.msg) != false) {
            rep_op.applyOperation();
            sqi.recv_sibling->rcvd_success++;
            // note no port because it can be ephemeral
            // (thus rcvd stats are tracked only on a per-IP basis)
            incPrometheusReplicationRcvd(sqi.remote.toString(), true);
          }
          else {
            errlog("Invalid replication operation received from %s", sqi.remote.toString());
            sqi.recv_sibling->rcvd_fail++;
            incPrometheusReplicationRcvd(sqi.remote.toString(), false);

          }
        }
      });
    t.detach();
  }
}

void parseReceivedReplicationMsg(const std::string& msg, const ComboAddress& remote, std::shared_ptr<Sibling> recv_sibling)
{
  SiblingQueueItem sqi = { msg, remote, recv_sibling };
  {
    std::lock_guard<std::mutex> lock(g_sibling_queue_mutex);
    if (g_sibling_queue.size() >= max_sibling_queue_size) {
      errlog("parseReceivedReplicationMsg: max sibling queue size (%d) reached - dropping replication msg", max_sibling_queue_size);
      return;
    }
    else {
      g_sibling_queue.push(sqi);
    }
  }
  g_sibling_queue_cv.notify_one();
}

void parseTCPReplication(std::shared_ptr<Socket> sockp, const ComboAddress& remote, std::shared_ptr<Sibling> recv_sibling)
{
  setThreadName("wf/repl-tcp");

  infolog("New TCP Replication connection from %s", remote.toString());
  uint16_t size;
  ssize_t ssize = static_cast<ssize_t>(sizeof(size));
  char buffer[65535];
  ssize_t len;
  unsigned int num_rcvd=0;
  
  try {
    while(true) {
      len = sockp->read((char*)&size, ssize);
      if (len != ssize) {
        if (len)
          errlog("parseTCPReplication: error reading size, got %d bytes, but was expecting %d bytes", len, ssize);
        break;
      }
      size = ntohs(size);
      if (!size) {
        errlog("parseTCPReplication: Zero-length size field");
        break;
      }
      if (size > sizeof(buffer)) {
        errlog("parseTCPReplication: This should not happen - asked to read more than 65535 bytes");
        break;
      }
      int size_read = 0;
      if ((size_read = sockp->readAll(buffer, size)) != size) {
        errlog("parseTCPReplication: Told to read %d bytes, but actually read %d bytes", size, size_read);
        break;
      }
      std::string msg;
      if (!decryptMsg(buffer, size_read, msg)) {
        recv_sibling->rcvd_fail++;
        continue;
      }
      parseReceivedReplicationMsg(msg, remote, recv_sibling);
      num_rcvd++;
    }
    infolog("Received %d Replication entries from %s", num_rcvd, remote.toString());
  }
  catch(std::exception& e) {
    errlog("ParseTCPReplication: client thread died with error: %s", e.what());
  }
}

void receiveReplicationOperationsTCP(const ComboAddress& local)
{
  Socket sock(local.sin4.sin_family, SOCK_STREAM, 0);
  ComboAddress remote=local;

  setThreadName("wf/rcv-repl-tcp");
  
  sock.setReuseAddr();
  sock.bind(local);
  sock.listen(1024);
  noticelog("Launched TCP sibling replication listener on %s", local.toStringWithPort());
  
  for (;;) {
    // we wait for activity
    try {
      shared_ptr<Sibling> recv_sibling = nullptr;
      std::shared_ptr<Socket> connp(sock.accept());
      connp->setKeepAlive();
      if (connp->getRemote(remote) && !checkConnFromSibling(remote, recv_sibling)) {
        continue;
      }
      thread t1(parseTCPReplication, connp, remote, recv_sibling);
      t1.detach();
    }
    catch(std::exception& e) {
      errlog("receiveReplicationOperationsTCP: error accepting new connection: %s", e.what());
    }
  }
}

void receiveReplicationOperations(const ComboAddress& local)
{
  Socket sock(local.sin4.sin_family, SOCK_DGRAM);
  sock.bind(local);
  char buf[1500];
  ComboAddress remote=local;
  socklen_t remlen=remote.getSocklen();
  int len;
  auto siblings = g_siblings.getLocal();

  setThreadName("wf/rcv-repl-udp");
  
  noticelog("Launched UDP sibling replication listener on %s", local.toStringWithPort());
  for(;;) {
    shared_ptr<Sibling> recv_sibling = nullptr;
    len=recvfrom(sock.getHandle(), buf, sizeof(buf), 0, (struct sockaddr*)&remote, &remlen);
    if(len <= 0 || len >= (int)sizeof(buf))
      continue;

    if (!checkConnFromSibling(remote, recv_sibling)) {
      continue;
    }
    
    string msg;
    
    if (!decryptMsg(buf, len, msg)) {
      recv_sibling->rcvd_fail++;
      continue;
    }
    parseReceivedReplicationMsg(msg, remote, recv_sibling);
  }
}

void setMaxSiblingRecvQueueSize(size_t size)
{
  max_sibling_queue_size = size;
}
