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

#pragma once
#include "ext/luawrapper/include/LuaContext.hpp"
#include <time.h>
#include "misc.hh"
#include "iputils.hh"
#include <mutex>
#include <thread>
#include <queue>
#include <condition_variable>
#include <atomic>
#include "sholder.hh"
#include "sstuff.hh"

// Represents a replication destination/source
// All methods in this class are thread-safe
struct Sibling
{
  enum class Protocol : int { UDP=SOCK_DGRAM, TCP=SOCK_STREAM, NONE=-1 };
  explicit Sibling(const ComboAddress& ca);
  explicit Sibling(const ComboAddress& ca, const Protocol& p, int timeout=1000, size_t queue_size=5000);
  ~Sibling();
  Sibling(const Sibling&) = delete;
  ComboAddress rem;
  std::mutex mutx;
  std::unique_ptr<Socket> sockp;
  Protocol proto = Protocol::UDP;
  int connect_timeout; // milliseconds

  // The queue is used to process messages in a separate thread
  // so that replication delays/timeouts don't affect the caller.
  // To use this asynchronous behaviour, queueMsg() must be used.
  // To send synchronously, use send().
  std::mutex queue_mutx;
  std::queue<std::string> queue;
  std::condition_variable queue_cv;
  size_t max_queue_size;
  std::thread queue_thread; // We hang on to this so that the queue thread can be terminated in the destructor
  bool queue_thread_run;
  
  std::atomic<unsigned int> success{0};
  std::atomic<unsigned int> failures{0};
  std::atomic<unsigned int> rcvd_fail{0};
  std::atomic<unsigned int> rcvd_success{0};
  void send(const std::string& msg);
  void queueMsg(const std::string& msg);
  void checkIgnoreSelf(const ComboAddress& ca);
  void connectSibling();
  void setConnectTimeout(int timeout);
  void setMaxQueueSize(size_t queue_size);
  static Protocol stringToProtocol(const std::string& s) {
    if (s.compare("tcp") == 0)
      return Sibling::Protocol::TCP;
    else
      return Sibling::Protocol::UDP;
  }
  static std::string protocolToString(const Protocol& p) {
    if (p == Protocol::TCP)
      return std::string("tcp");
    else
      return std::string("udp");
  }
  bool d_ignoreself{false};
};

void setMaxSiblingSendQueueSize(size_t queue_size);
void setSiblingConnectTimeout(int timeout); // milliseconds
void parseSiblingString(const std::string &str, std::string &ca_str, Sibling::Protocol &proto);
void removeSibling(const std::string& address,
                    GlobalStateHolder<vector<shared_ptr<Sibling>>> &siblings,
                    std::string &output_buffer);
void addSibling(const std::string& address,
                 GlobalStateHolder<vector<shared_ptr<Sibling>>> &siblings,
                 std::string &output_buffer);
void setSiblings(const vector<pair<int, string>> &parts,
                 GlobalStateHolder<vector<shared_ptr<Sibling>>> &siblings,
                 std::string &output_buffer);
