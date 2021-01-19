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
#include "wforce-sibling.hh"
#include "wforce_exception.hh"
#include "wforce_ns.hh"
#include <stddef.h>

#define SYSLOG_NAMES

#include <syslog.h>
#include "sstuff.hh"
#include "misc.hh"
#include <netinet/tcp.h>
#include <limits>
#include "dolog.hh"
#include <unistd.h>
#include "sodcrypto.hh"
#include "iputils.hh"
#include "base64.hh"
#include "ext/threadname.hh"
#include "wforce-prometheus.hh"

using std::atomic;
using std::thread;

static std::atomic<int> sibling_connect_timeout(5000); // milliseconds
static std::atomic<size_t> sibling_queue_size(5000);

Sibling::Sibling(const ComboAddress& ca) : Sibling(ca, Protocol::UDP)
{
}

Sibling::Sibling(const ComboAddress& ca,
                 const Protocol& p,
                 int timeout,
                 size_t queue_size) : Sibling(ca, p, std::string(), timeout, queue_size)
{
}

void Sibling::connectSibling()
{
  sockp = wforce::make_unique<Socket>(rem.sin4.sin_family, static_cast<int>(proto));
  if (proto == Protocol::UDP) {
    sockp->connect(rem);
  }
  else if (proto == Protocol::TCP) {
    if (!d_ignoreself) {
      try {
        sockp->setKeepAlive();
        sockp->connectWithTimeout(rem, connect_timeout);
      }
      catch (const NetworkError& e) {
        errlog("TCP Connect to Sibling %s failed (%s)", rem
            .toStringWithPort(), e.what());
        incPrometheusReplicationConnFail(rem.toStringWithPort());
      }
    }
  }
}

Sibling::Sibling(const ComboAddress& ca,
                 const Protocol& p,
                 const std::string& key,
                 int timeout,
                 size_t queue_size) : rem(ca), proto(p),
                                      connect_timeout(timeout),
                                      max_queue_size(queue_size),
                                      queue_thread_run(true),
                                      d_ignoreself(false),
                                      d_key(key)
{
  if (!d_key.empty()) {
    d_has_key = true;
  }
  if (proto != Protocol::NONE) {
    {
      std::lock_guard<std::mutex> lock(mutx);
      connectSibling();
    }
    // std::thread is moveable
    queue_thread = std::thread([this]() {
      thread_local bool init = false;
      if (!init) {
        setThreadName("wf/sibling-worker");
        init = true;
      }
      while (true) {
        std::string msg;
        {
          std::unique_lock<std::mutex> lock(queue_mutx);
          while ((queue.size() == 0) && queue_thread_run) {
            queue_cv.wait(lock);
          }
          if (!queue_thread_run)
            return;
          msg = std::move(queue.front());
          queue.pop();
        }
        send(msg);
      }
    });
  }
}

Sibling::~Sibling()
{
  if (proto != Protocol::NONE) {
    {
      std::lock_guard<std::mutex> lock(queue_mutx);
      queue_thread_run = false;
    }
    queue_cv.notify_one();
    queue_thread.join();
  }
}

void Sibling::checkIgnoreSelf(const ComboAddress& ca)
{
  if (proto != Protocol::NONE) {
    ComboAddress actualLocal;
    actualLocal.sin4.sin_family = ca.sin4.sin_family;
    socklen_t socklen = actualLocal.getSocklen();

    if (getsockname(sockp->getHandle(), (struct sockaddr*) &actualLocal, &socklen) < 0) {
      return;
    }

    actualLocal.sin4.sin_port = ca.sin4.sin_port;
    if (actualLocal == rem) {
      d_ignoreself = true;
    }
  }
}

void Sibling::send(const std::string& msg)
{
  if (d_ignoreself)
    return;

  if (proto == Protocol::UDP) {
    if (::send(sockp->getHandle(), msg.c_str(), msg.length(), 0) <= 0) {
      ++failures;
      incPrometheusReplicationSent(rem.toStringWithPort(), false);
    }
    else {
      ++success;
      incPrometheusReplicationSent(rem.toStringWithPort(), true);
    }
  }
  else if (proto == Protocol::TCP) {
    // This needs protecting with a mutex because of the reconnect logic
    std::lock_guard<std::mutex> lock(mutx);
    // Try to send. In case of error, try to reconnect (but only once) and then try to send again
    for (int i = 0; i < 2; ++i) {
      uint16_t nsize = htons(msg.length());
      try {
        sockp->writen(std::string((char*) &nsize, sizeof(nsize)));
        sockp->writen(msg);
        ++success;
        incPrometheusReplicationSent(rem.toStringWithPort(), true);
        break;
      }
      catch (const NetworkError& e) {
        if (i == 0) {
          ++failures;
          incPrometheusReplicationSent(rem.toStringWithPort(), false);
          errlog("Error writing to Sibling %s, reconnecting (%s)", rem.toStringWithPort(), e.what());
          connectSibling();
        }
      }
    }
  }
}

void Sibling::queueMsg(const std::string& msg)
{
  {
    std::lock_guard<std::mutex> lock(queue_mutx);
    if (queue.size() >= max_queue_size) {
      errlog("Sibling::queueMsg: max sibling send queue size (%d) reached - dropping replication msg", max_queue_size);
      return;
    }
    else {
      queue.push(msg);
    }
  }
  queue_cv.notify_one();
}

void parseSiblingString(const std::string& str, std::string& ca_str, Sibling::Protocol& proto)
{
  std::vector<std::string> sres;
  boost::split(sres, str, boost::is_any_of(":"));

  if (sres.size() > 3) {
    throw WforceException("Malformed sibling string: " + str + "(Format is <host>[:<port>[:<protocol>]]");
  }
  else if (sres.size() == 3) {
    proto = Sibling::stringToProtocol(sres.back());
    sres.pop_back();
    ca_str = boost::join(sres, ":");
  }
  else {
    proto = Sibling::Protocol::UDP;
    ca_str = str;
  }
}

void removeSibling(const std::string& address,
                   GlobalStateHolder<vector<shared_ptr<Sibling>>>& siblings,
                   std::string& output_buffer)
{
  ComboAddress ca;
  std::string ca_str;
  Sibling::Protocol proto;
  parseSiblingString(address, ca_str, proto);
  try {
    ca = ComboAddress(ca_str, 4001);
  }
  catch (const WforceException& e) {
    const std::string errstr = (boost::format("%s [%s]. %s (%s)\n") % "removeSibling() error parsing address/port" %
                                address % "Make sure to use IP addresses not hostnames" % e.reason).str();
    errlog(errstr.c_str());
    output_buffer += errstr;
    return;
  }
  siblings.modify([ca, proto](vector<shared_ptr<Sibling>>& v) {
    for (auto i = v.begin(); i != v.end();) {
      if ((i->get()->rem == ca) && (i->get()->proto == proto)) {
        i = v.erase(i);
        break;
      }
      else {
        i++;
      }
    }
  });

  bool found = false;
  auto local_siblings = siblings.getLocal();
  for (auto& s : *local_siblings) {
    if (s->rem == ca) {
      found = true;
      break;
    }
  }
  if (!found) {
    removePrometheusReplicationSibling(ca.toStringWithPort());
  }
  found = false;
  for (auto& s : *local_siblings) {
    if (ComboAddress::addressOnlyEqual()(s->rem, ca)) {
      found = true;
      break;
    }
  }
  if (!found) {
    removePrometheusReplicationSibling(ca.toString());
  }
}

bool addSibling(const std::string& address,
                GlobalStateHolder<vector<shared_ptr<Sibling>>>& siblings,
                std::string& output_buffer)
{
  return addSiblingWithKey(address, siblings, output_buffer, std::string());
}

bool addSiblingWithKey(const std::string& address,
                       GlobalStateHolder<vector<shared_ptr<Sibling>>>& siblings,
                       std::string& output_buffer,
                       const std::string& key)
{
  ComboAddress ca;
  std::string ca_str;
  Sibling::Protocol proto;

  string raw_key;
  if(B64Decode(key, raw_key) < 0) {
    output_buffer+=string("Unable to decode ")+key+" as Base64";
    errlog("%s", output_buffer);
    return false;
  }

  parseSiblingString(address, ca_str, proto);
  try {
    ca = ComboAddress(ca_str, 4001);
  }
  catch (const WforceException& e) {
    const std::string errstr = (boost::format("%s [%s]. %s (%s)\n") % "addSibling() error parsing address/port" %
                                address % "Make sure to use IP addresses not hostnames" % e.reason).str();
    errlog(errstr.c_str());
    output_buffer += errstr;
    return false;
  }
  // This is for sending when we know the port
  addPrometheusReplicationSibling(ca.toStringWithPort());
  // This is for receiving when the port may be ephemeral
  addPrometheusReplicationSibling(ca.toString());
  siblings.modify([ca, proto, raw_key](vector<shared_ptr<Sibling>>& v) {
    v.push_back(std::make_shared<Sibling>(ca, proto, raw_key, sibling_connect_timeout, sibling_queue_size));
  });
  return true;
}

bool setSiblings(const vector<std::pair<int, string>>& parts,
                 GlobalStateHolder<vector<shared_ptr<Sibling>>>& siblings,
                 std::string& output_buffer)
{
  std::vector<std::pair<int, std::vector<std::pair<int, std::string>>>> t_vec;
  for (auto& i : parts) {
    t_vec.emplace_back(std::pair<int, std::vector<std::pair<int, std::string>>>{i.first, { { 0, i.second }, { 1, std::string()}}});
  }
  return setSiblingsWithKey(t_vec, siblings, output_buffer);
}

bool setSiblingsWithKey(const std::vector<std::pair<int, std::vector<std::pair<int, std::string>>>>& parts,
                        GlobalStateHolder<vector<shared_ptr<Sibling>>>& siblings,
                        std::string& output_buffer)
{
  removeAllPrometheusReplicationSiblings();
  vector<shared_ptr<Sibling>> v;
  for (const auto& p : parts) {
    if (p.second.size() != 2) {
      errlog("setSiblings[WithKey](): Invalid values - was expecting 2 args, got %d", p.second.size());
      output_buffer += "Invalid number of args to setSiblings[WithKey] - was expecting 2 (sibling address & key)";
      return false;
    }
    try {
      std::string ca_string;
      Sibling::Protocol proto;

      string raw_key;
      if(B64Decode(p.second[1].second, raw_key) < 0) {
        output_buffer+=string("Unable to decode ")+p.second[1].second+" as Base64";
        errlog("%s", output_buffer);
        return false;
      }
      parseSiblingString(p.second[0].second, ca_string, proto);
      // This is for sending when we know the port
      addPrometheusReplicationSibling(ComboAddress(ca_string, 4001).toStringWithPort());
      // This is for receiving when the port may be ephemeral
      addPrometheusReplicationSibling(ComboAddress(ca_string, 4001).toString());
      // Create the sibling after the Prometheus metrics so connfail stats get updated
      v.push_back(
          std::make_shared<Sibling>(ComboAddress(ca_string, 4001), proto, raw_key, sibling_connect_timeout,
                                    sibling_queue_size));
    }
    catch (const WforceException& e) {
      const std::string errstr = (boost::format("%s [%s]. %s (%s)\n") % "setSiblings() error parsing address/port" %
                                  p.second[0].second % "Make sure to use IP addresses not hostnames" % e.reason).str();
      errlog(errstr.c_str());
      output_buffer += errstr;
      return false;
    }
  }
  siblings.setState(v);
  return true;
}

void setMaxSiblingSendQueueSize(size_t queue_size)
{
  sibling_queue_size.store(queue_size);
}

void setSiblingConnectTimeout(int timeout)
{
  sibling_connect_timeout.store(timeout);
}
