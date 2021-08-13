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
#include <queue>
#include <unordered_map>
#include <ctime>
#include <sys/resource.h>
#include "iputils.hh"
#include "sstuff.hh"
#include "sholder.hh"
#include "ext/ctpl.h"
#include "drogon/drogon.h"
#include "prometheus.hh"
#include "perf-stats.hh"

using WforceWSFuncPtr = void (*)(const drogon::HttpRequestPtr& req,
                                 const std::string& command,
                                 const drogon::HttpResponsePtr& resp);

struct WforceWSFunc {
  WforceWSFunc() = delete;

  explicit WforceWSFunc(WforceWSFuncPtr func) : d_func_ptr(func), d_ret_content_type(drogon::CT_APPLICATION_JSON)
  {}

  WforceWSFunc(WforceWSFuncPtr func, drogon::ContentType ct) : d_func_ptr(func), d_ret_content_type(ct)
  {}

  WforceWSFuncPtr d_func_ptr;
  drogon::ContentType d_ret_content_type;
};

enum class HTTPVerb {
  GET, POST, PUT, DELETE
};

#define WFORCE_NUM_WORKER_THREADS 4
#define WFORCE_MAX_WS_CONNS 10000

class LoginFilter : public drogon::HttpFilter<LoginFilter, false> {
public:
  explicit LoginFilter(const std::string& pass) : d_password(pass)
  {}

  virtual void doFilter(const drogon::HttpRequestPtr& req,
                        drogon::FilterCallback&& fcb,
                        drogon::FilterChainCallback&& fccb) override;

  static bool compareAuthorization(const std::string& auth_header, const string& expected_password);

private:
  const std::string d_password;
};

class ACLFilter : public drogon::HttpFilter<ACLFilter, false> {
public:
  explicit ACLFilter(GlobalStateHolder<NetmaskGroup>& acl) : d_ACL(acl.getLocal())
  {}

  virtual void doFilter(const drogon::HttpRequestPtr& req,
                        drogon::FilterCallback&& fcb,
                        drogon::FilterChainCallback&& fccb) override;

private:
  LocalStateHolder<NetmaskGroup> d_ACL;
};

class WforceWebserver {
public:
  WforceWebserver() = default;

  // detect attempts to copy at compile time
  WforceWebserver(const WforceWebserver&) = delete;

  WforceWebserver& operator=(const WforceWebserver&) = delete;

  // timeout for poll()
  void setNumWorkerThreads(unsigned int num_workers);

  void setMaxConns(unsigned int max_conns);

  // set the ACLs
  void setACL(const NetmaskGroup& nmg);

  void addACL(const std::string& ip);

  NetmaskGroup getACL();

  size_t getNumConns();

  // set the basic-auth password
  void setBasicAuthPassword(const std::string& password)
  {
    d_password = password;
  }

  // Register functions to parse commands
  bool registerFunc(const std::string& command, HTTPVerb verb, const WforceWSFunc& func);

  void addSimpleListener(const std::string& ip, unsigned int port)
  {
    infolog("Adding webserver listener on %s:%d", ip, port);
    drogon::app().addListener(ip, port);
  }

  void addListener(const std::string& ip, unsigned int port, bool use_ssl,
                   const std::string& cert_file, const std::string& private_key,
                   bool enable_oldtls, std::vector<std::pair<std::string, std::string>> opts);

  void setWebLogLevel(trantor::Logger::LogLevel level)
  {
    drogon::app().setLogLevel(level);
  }

  // Initialize the webserver
  void init()
  {
    static std::atomic_flag init = false;

    if (init.test_and_set() == false) {
      drogon::app().disableSession();
      drogon::app().disableSigtermHandling();
      // Set custom 404 response
      auto resp = drogon::HttpResponse::newHttpResponse();
      resp->setBody(R"({"status":"failure", "reason":"Not found"})");
      drogon::app().setCustom404Page(resp, true);
      // register ACLFilter
      drogon::app().registerFilter(std::make_shared<ACLFilter>(d_ACL));
      // register LoginFilter
      drogon::app().registerFilter(std::make_shared<LoginFilter>(d_password));
      // register prometheus metrics handler
      drogon::app().registerHandler("/metrics",
                                    [](const drogon::HttpRequestPtr& req,
                                       std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
                                      auto res = drogon::HttpResponse::newHttpResponse();
                                      res->setBody(serializePrometheusMetrics());
                                      res->setContentTypeCode(drogon::CT_TEXT_PLAIN);
                                      res->setStatusCode(drogon::k200OK);
                                      callback(res);
                                    },
                                    {drogon::Get, "ACLFilter", "LoginFilter"});
      drogon::app().setThreadNum(d_num_worker_threads);
      drogon::app().setMaxConnectionNum(d_max_conns);
      // register handlers for old-style /?command=<blah> paths
      auto handler_block = [this](const drogon::HttpRequestPtr& req,
                                  std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                                  const std::string& command,
                                  std::unordered_map<std::string, WforceWSFunc>& map) {
        auto start_time = std::chrono::steady_clock::now();
        auto resp = drogon::HttpResponse::newHttpResponse();
        const auto& f = map.find(command);
        if (f != map.end()) {
          resp->setContentTypeCode(f->second.d_ret_content_type);
          f->second.d_func_ptr(req, command, resp);
        } else {
          resp->setStatusCode(drogon::k404NotFound);
          resp->setBody(R"({"status":"failure", "reason":"Not found"})");
        }
        callback(resp);
        updateWTR(start_time);
      };
      drogon::app().registerHandler("/?command={command}",
                                    [this, handler_block](const drogon::HttpRequestPtr& req,
                                           std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                                           const std::string& command) {
        handler_block(req, std::forward<std::function<void(const drogon::HttpResponsePtr&)>>(callback), command, d_get_map);
                                    },
                                    {drogon::Get, "ACLFilter", "LoginFilter"});
      drogon::app().registerHandler("/?command={command}",
                                    [this, handler_block](const drogon::HttpRequestPtr& req,
                                           std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                                           const std::string& command) {
        handler_block(req, std::forward<std::function<void(const drogon::HttpResponsePtr&)>>(callback), command, d_post_map);
                                    },
                                    {drogon::Post, "ACLFilter", "LoginFilter"});
      drogon::app().registerHandler("/?command={command}",
                                    [this, handler_block](const drogon::HttpRequestPtr& req,
                                           std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                                           const std::string& command) {
        handler_block(req, std::forward<std::function<void(const drogon::HttpResponsePtr&)>>(callback), command, d_put_map);
                                    },
                                    {drogon::Put, "ACLFilter", "LoginFilter"});
      drogon::app().registerHandler("/?command={command}",
                                    [this, handler_block](const drogon::HttpRequestPtr& req,
                                           std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                                           const std::string& command) {
        handler_block(req, std::forward<std::function<void(const drogon::HttpResponsePtr&)>>(callback), command, d_delete_map);
                                    },
                                    {drogon::Delete, "ACLFilter", "LoginFilter"});
    }
  }

  static void start(WforceWebserver* wws)
  {
    static std::atomic_flag init = false;
    if (init.test_and_set() == false) {
      infolog("Starting webserver");
      wws->init();
      drogon::app().run();
    }
  }

protected:
  template <typename Clock>
  void updateWTR(const std::chrono::time_point<Clock>& start_time)
  {
    auto end_time = std::chrono::steady_clock::now();
    auto run_time = end_time - start_time;
    auto i_millis = std::chrono::duration_cast<std::chrono::milliseconds>(run_time);
    addWTRStat(i_millis.count());
    observePrometheusWRD(std::chrono::duration<float>(run_time).count());
  }

private:
  GlobalStateHolder<NetmaskGroup> d_ACL;
  std::string d_password;
  std::unordered_map<std::string, WforceWSFunc> d_get_map;
  std::unordered_map<std::string, WforceWSFunc> d_post_map;
  std::unordered_map<std::string, WforceWSFunc> d_put_map;
  std::unordered_map<std::string, WforceWSFunc> d_delete_map;
  unsigned int d_num_worker_threads = WFORCE_NUM_WORKER_THREADS;
  unsigned int d_max_conns = WFORCE_MAX_WS_CONNS;
};
