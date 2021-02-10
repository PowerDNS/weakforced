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
#include "sodcrypto.hh"

// Represents a replication destination/source
// All methods in this class are thread-safe
struct Sibling {
  enum class Protocol : int {
    UDP = SOCK_DGRAM, TCP = SOCK_STREAM, NONE = -1
  };

  explicit Sibling(const ComboAddress& ca);

  explicit Sibling(const ComboAddress& ca, const Protocol& p, int timeout = 1000, size_t queue_size = 5000, bool sdb_send=true, bool wlbl_send=true);

  explicit Sibling(const ComboAddress& ca, const Protocol& p, const std::string& key, int timeout = 1000,
                   size_t queue_size = 5000, bool sdb_send=true, bool wlbl_send=true);

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

  // Whether to send replication and/or wlbl events
  bool sdb_send;
  bool wlbl_send;

  std::atomic<unsigned int> success{0};
  std::atomic<unsigned int> failures{0};
  std::atomic<unsigned int> rcvd_fail{0};
  std::atomic<unsigned int> rcvd_success{0};

  void send(const std::string& msg);

  void queueMsg(const std::string& msg);

  void checkIgnoreSelf(const ComboAddress& ca);

  void connectSibling();

  static Protocol stringToProtocol(const std::string& s)
  {
    if (s.compare("tcp") == 0)
      return Sibling::Protocol::TCP;
    else if (s.compare("udp") == 0)
      return Sibling::Protocol::UDP;
    else if (s.compare("none") == 0)
      return Sibling::Protocol::NONE;
    else if (s.empty())
      return Sibling::Protocol::UDP; // Default to UDP if no protocol supplied
    else {
      std::string err = "Sibling::stringToProtocol(): Unknown protocol " + s;
      throw WforceException(err);
    }
  }

  static std::string protocolToString(const Protocol& p)
  {
    if (p == Protocol::TCP)
      return std::string("tcp");
    else if (p == Protocol::UDP)
      return std::string("udp");
    else if (p == Protocol::NONE)
      return std::string("none");
    else
      throw WforceException("Sibling::protocolToString(): unknown protocol");
  }

  bool d_ignoreself{false};

  // Optional per-sibling encryption - if key is not empty then
  // it and the nonce will be used, otherwise global key & nonce are used
  bool d_has_key = false;
  std::string d_base64_key;
  std::string d_key;
  SodiumNonce d_nonce;
};

void setMaxSiblingSendQueueSize(size_t queue_size);

void setSiblingConnectTimeout(int timeout); // milliseconds

std::string createSiblingAddress(const std::string& host, int port, Sibling::Protocol proto);

bool removeSibling(const std::string& host, int port,
                   GlobalStateHolder<vector<shared_ptr<Sibling>>>& siblings,
                   std::string& output_buffer);

bool removeSibling(const std::string& address,
                   GlobalStateHolder<vector<shared_ptr<Sibling>>>& siblings,
                   std::string& output_buffer);

bool addSibling(const std::string& address,
                GlobalStateHolder<vector<shared_ptr<Sibling>>>& siblings,
                std::string& output_buffer, bool send_sdb=true, bool send_wlbl=true);

bool addSibling(const std::string& host, int port, Sibling::Protocol proto,
                GlobalStateHolder<vector<shared_ptr<Sibling>>>& siblings,
                std::string& output_buffer, bool send_sdb=true, bool send_wlbl=true);

bool addSiblingWithKey(const std::string& host, int port, Sibling::Protocol proto,
                       GlobalStateHolder<vector<shared_ptr<Sibling>>>& siblings,
                       std::string& output_buffer,
                       const std::string& key, bool send_sdb=true, bool send_wlbl=true);

bool addSiblingWithKey(const std::string& address,
                       GlobalStateHolder<vector<shared_ptr<Sibling>>>& siblings,
                       std::string& output_buffer,
                       const std::string& key, bool send_sdb=true, bool send_wlbl=true);

bool setSiblings(const vector<pair<int, string>>& parts,
                 GlobalStateHolder<vector<shared_ptr<Sibling>>>& siblings,
                 std::string& output_buffer);

bool setSiblingsWithKey(const vector<std::pair<int, std::vector<std::pair<int, std::string>>>>& parts,
                        GlobalStateHolder<vector<shared_ptr<Sibling>>>& siblings,
                        std::string& output_buffer);
