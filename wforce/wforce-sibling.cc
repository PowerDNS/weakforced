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
  else {
    if (!d_ignoreself) {
      try {
        sockp->connect(rem);
      }
      catch (const NetworkError& e) {
        errlog("TCP Connect to Sibling %s failed (%s)", rem
                  .toStringWithPort(), e.what());
      }
    }
  }
}

Sibling::Sibling(const ComboAddress& ca, Protocol p) : rem(ca), proto(p), d_ignoreself(false)
{
  std::thread t([this]() {
      thread_local bool init=false;
      if (!init) {
        setThreadName("wf/sibling-worker");
        init = true;
      }
      connectSibling();
      while (true) {
        std::string msg;
        {
          std::unique_lock<std::mutex> lock(queue_mutx);
          while (queue.size() == 0) {
            queue_cv.wait(lock);
          }
          msg = std::move(queue.front());
          queue.pop();
        }
        send(msg);
      }
    });
  t.detach();
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
    }
    else
      ++success;
  }
  else if (proto == Protocol::TCP) {
    // This needs protecting with a mutex because of the reconnect logic
    std::lock_guard<std::mutex> lock(mutx);

    uint16_t nsize = htons(msg.length());
    try {
      sockp->writen(std::string((char*)&nsize, sizeof(nsize)));
      sockp->writen(msg);
      ++success;
    }
    catch (const NetworkError& e) {
      ++failures;
      errlog("Error writing to Sibling %s, reconnecting (%s)", rem.toStringWithPort(), e.what());
      connectSibling();
    }
  }
}

void Sibling::queueMsg(const std::string& msg)
{
  {
    std::lock_guard<std::mutex> lock(queue_mutx);
    if (queue.size() >= max_queue_size) {
      errlog("Sibling::queueMsg: max sibling queue size (%d) reached - dropping replication msg");
      return;
    }
    else {
      queue.push(msg);
    }
  }
  queue_cv.notify_one();
}
