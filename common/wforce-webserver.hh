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

#include <chrono>
#include <thread>
#include <sstream>
#include <mutex>
#include <map>
#include <unordered_map>
#include <sys/time.h>
#include <sys/resource.h>
#include "iputils.hh"
#include "sstuff.hh"
#include "sholder.hh"
#include "ext/ctpl.h"
#include "yahttp/yahttp.hpp"

struct WFConnection
{
  WFConnection(int sock, const ComboAddress& ca, const std::string pass) : s(sock)
  {
    fd = sock;
    remote = ca;
    password = pass;
    closeConnection = false;
    inConnectionThread = false;
  }
  bool inConnectionThread;
  bool closeConnection;
  int fd;
  Socket s;
  ComboAddress remote;
  std::string password;
  std::chrono::time_point<std::chrono::steady_clock> q_entry_time;
};

typedef std::vector<std::shared_ptr<WFConnection>> WFCArray;

using WforceWSFunc = void (*)(const YaHTTP::Request&, YaHTTP::Response&, const std::string& path);

enum class HTTPVerb { GET, POST, PUT, DELETE };

#define WFORCE_NUM_WORKER_THREADS 4

class WforceWebserver {
public:
  WforceWebserver()
  {	
    d_content_type = "application/json";
  }
  // detect attempts to copy at compile time
  WforceWebserver(const WforceWebserver&) = delete;
  WforceWebserver& operator=(const WforceWebserver&) = delete;

  // timeout for poll()
  void setPollTimeout(int timeout_ms);
  void setNumWorkerThreads(unsigned int num_workers);
  // What the expected content-type is (for POST/PUT commands)
  void setContentType(const std::string& content_type);
  // set the ACLs
  void setACL(const NetmaskGroup& nmg);
  void addACL(const std::string& ip);
  NetmaskGroup getACL();
  
  // Register functions to parse commands
  bool registerFunc(const std::string& command, HTTPVerb verb, WforceWSFunc func);

  // Start listening - this function doesn't return, so start in a thread
  // if you want to do other stuff
  static void start(int sock, const ComboAddress& local, const std::string& password, WforceWebserver* wws);
protected:
  static void connectionThread(int id, std::shared_ptr<WFConnection> wfc, WforceWebserver* wws);
  static bool compareAuthorization(YaHTTP::Request& req, const string &expected_password);
  static void pollThread(WforceWebserver* wws);
private:
  int d_poll_timeout = 5;
  unsigned int d_num_worker_threads = WFORCE_NUM_WORKER_THREADS;
  std::string d_content_type;
  std::unordered_map<std::string, WforceWSFunc> d_get_map;
  std::unordered_map<std::string, WforceWSFunc> d_post_map;
  std::unordered_map<std::string, WforceWSFunc> d_put_map;
  std::unordered_map<std::string, WforceWSFunc> d_delete_map;
  GlobalStateHolder<NetmaskGroup> d_ACL;
  WFCArray d_sock_vec;
  mutable std::mutex d_sock_vec_mutx;
};
