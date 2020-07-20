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
#include "ext/threadname.hh"
#include "wforce-prometheus.hh"

using std::atomic;
using std::thread;

Sibling::Sibling(const ComboAddress& ca) : Sibling(ca, Protocol::UDP)
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
        sockp->connect(rem);
      }
      catch (const NetworkError& e) {
        errlog("TCP Connect to Sibling %s failed (%s)", rem
                  .toStringWithPort(), e.what());
        incPrometheusReplicationConnFail(rem.toStringWithPort());
      }
    }
  }
}

Sibling::Sibling(const ComboAddress& ca, const Protocol& p) : rem(ca), proto(p), queue_thread_run(true), d_ignoreself(false)
{
  if (proto != Protocol::NONE) {
    connectSibling();
    // std::thread is moveable
    queue_thread = std::thread([this]() {
        thread_local bool init=false;
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
  ComboAddress actualLocal;
  actualLocal.sin4.sin_family = ca.sin4.sin_family;
  socklen_t socklen = actualLocal.getSocklen();

  if(getsockname(sockp->getHandle(), (struct sockaddr*) &actualLocal, &socklen) < 0) {
    return;
  }

  actualLocal.sin4.sin_port = ca.sin4.sin_port;
  if(actualLocal == rem) {
    d_ignoreself=true;
  }
}

void Sibling::send(const std::string& msg)
{
  if(d_ignoreself)
    return;

  if (proto == Protocol::UDP) {
    if(::send(sockp->getHandle(),msg.c_str(), msg.length(),0) <= 0) {
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
    for (int i=0; i<2; ++i) {
      uint16_t nsize = htons(msg.length());
      try {
        sockp->writen(std::string((char*)&nsize, sizeof(nsize)));
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
      errlog("Sibling::queueMsg: max sibling queue size (%d) reached - dropping replication msg", max_queue_size);
      return;
    }
    else {
      queue.push(msg);
    }
  }
  queue_cv.notify_one();
}
