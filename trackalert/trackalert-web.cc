/*
 * This file is part of PowerDNS or weakforced.
 * Copyright -- PowerDNS.COM B.V. and its contributors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
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

#include "trackalert-web.hh"
#include "wforce-webserver.hh"
#include "wforce_ns.hh"
#include "trackalert.hh"
#include "json11.hpp"
#include "ext/incbin/incbin.h"
#include "dolog.hh"
#include <chrono>
#include <atomic>
#include <thread>
#include <sstream>
#include "yahttp/yahttp.hpp"
#include "namespaces.hh"
#include <sys/time.h>
#include <sys/resource.h>
#include "ext/ctpl.h"
#include "base64.hh"
#include "perf-stats.hh"
#include "trackalert-luastate.hh"
#include "webhook.hh"
#include "login_tuple.hh"

std::atomic<int> numReportThreads(TRACKALERT_NUM_REPORT_THREADS);

static int uptimeOfProcess()
{
  static time_t start=time(0);
  return time(0) - start;
}

void setNumReportThreads(int numThreads)
{
  numReportThreads = numThreads;
}

void initReportThreadPool()
{
  }

void reportLog(const LoginTuple& lt)
{
  if (g_verbose) {
    std::ostringstream os;
    os << "reportLog: ";
    os << "remote=\"" << lt.remote.toString() << "\" ";
    os << "login=\"" << lt.login << "\" ";
    os << "success=\"" << lt.success << "\" ";
    os << "policy_reject=\"" << lt.policy_reject << "\" ";
    os << "pwhash=\"" << std::hex << std::uppercase << lt.pwhash << "\" ";
    os << "protocol=\"" << lt.protocol << "\" ";
    os << "device_id=\"" << lt.device_id << "\" ";
    os << DeviceAttrsToString(lt);
    os << LtAttrsToString(lt);
    infolog(os.str().c_str());
  }
}

void runReportLua(int id, Json msg, std::string command)
{
  try {
    LoginTuple lt;

    lt.from_json(msg, nullptr);
    reportLog(lt);
    g_stats.reports++;
    g_luamultip->report(lt);
  }
  catch(LuaContext::ExecutionErrorException& e) {
    try {
      std::rethrow_if_nested(e);
      errlog("Lua function [%s] exception: %s", command, e.what());
    }
    catch (const std::exception& ne) {
      errlog("Exception in command [%s] exception: %s", command, ne.what());
    }
    catch (const WforceException& ne) {
      errlog("Exception in command [%s] exception: %s", command, ne.reason);
    }
  }
  catch(const std::exception& e) {
    errlog("Exception in command [%s] exception: %s", command, e.what());
  }
  catch(const WforceException& e) {
    errlog("Exception in command [%s] exception: %s", command, e.reason);
  }
}

void parseReportCmd(const YaHTTP::Request& req, YaHTTP::Response& resp, const std::string& command)
{
  using namespace json11;
  Json msg;
  string err;
  static std::atomic<bool> init(false);
  static std::unique_ptr<ctpl::thread_pool> reportPoolp;

  msg=Json::parse(req.body, err);
  if (msg.is_null()) {
    resp.status=500;
    std::stringstream ss;
    ss << "{\"status\":\"failure\", \"reason\":\"" << err << "\"}";
    resp.body=ss.str();
  }
  else {
    if (!init) {
      reportPoolp = wforce::make_unique<ctpl::thread_pool>(numReportThreads, 5000);
      init = true;
    }
    if (reportPoolp) {
      reportPoolp->push(runReportLua, msg, command);
    }
    else {
      std::string myerr = "Cannot initialize report thread pool - this is a catastrophic error!";	
      errlog(myerr.c_str());
      resp.status = 500;
      std::stringstream ss;
      ss << "{\"status\":\"failure\", \"reason\":\"" << myerr << "\"}";
      resp.body=ss.str();
      return;
    }
    resp.status=200;
    resp.body=R"({"status":"ok"})";      
  }
}

void parseStatsCmd(const YaHTTP::Request& req, YaHTTP::Response& resp, const std::string& command)
{
  using namespace json11;
  struct rusage ru;
  getrusage(RUSAGE_SELF, &ru);

  resp.status=200;
  Json my_json = Json::object {
    { "reports", (int)g_stats.reports },
    { "user-msec", (int)(ru.ru_utime.tv_sec*1000ULL + ru.ru_utime.tv_usec/1000) },
    { "sys-msec", (int)(ru.ru_stime.tv_sec*1000ULL + ru.ru_stime.tv_usec/1000) },
    { "uptime", uptimeOfProcess()},
    { "perfstats", perfStatsToJson()}
  };

  resp.status=200;
  resp.body=my_json.dump();
}

void registerWebserverCommands()
{
  g_webserver.registerFunc("report", HTTPVerb::POST, parseReportCmd);
  g_webserver.registerFunc("stats", HTTPVerb::GET, parseStatsCmd);
}
