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
#include "namespaces.hh"
#include <ctime>
#include <sys/resource.h>
#include "base64.hh"
#include "perf-stats.hh"
#include "trackalert-luastate.hh"
#include "webhook.hh"
#include "login_tuple.hh"
#include "prometheus.hh"

static int uptimeOfProcess()
{
  static time_t start = time(0);
  return time(0) - start;
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

static void runReportLua(json11::Json msg, std::string command)
{
  try {
    LoginTuple lt;

    lt.from_json(msg, nullptr);
    reportLog(lt);
    g_stats.reports++;
    g_luamultip->report(lt);
  }
  catch (LuaContext::ExecutionErrorException& e) {
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
  catch (const std::exception& e) {
    errlog("Exception in command [%s] exception: %s", command, e.what());
  }
  catch (const WforceException& e) {
    errlog("Exception in command [%s] exception: %s", command, e.reason);
  }
}

void parseReportCmd(const drogon::HttpRequestPtr& req,
                    const std::string& command,
                    const drogon::HttpResponsePtr& resp)
{
  json11::Json msg;
  string err;

  msg = json11::Json::parse(req->bodyData(), err);
  if (msg.is_null()) {
    resp->setStatusCode(drogon::k500InternalServerError);
    std::stringstream ss;
    ss << "{\"status\":\"failure\", \"reason\":\"" << err << "\"}";
    resp->setBody(ss.str());
  }
  else {
    runReportLua(msg, command);
    resp->setStatusCode(drogon::k200OK);
    resp->setBody(R"({"status":"ok"})");
  }
  incCommandStat("report");
  incPrometheusCommandMetric("report");
}

void parseStatsCmd(const drogon::HttpRequestPtr& req,
                   const std::string& command,
                   const drogon::HttpResponsePtr& resp)
{
  struct rusage ru;
  getrusage(RUSAGE_SELF, &ru);

  json11::Json my_json = json11::Json::object{
      {"reports",   (int) g_stats.reports},
      {"user-msec", (int) (ru.ru_utime.tv_sec * 1000ULL + ru.ru_utime.tv_usec / 1000)},
      {"sys-msec",  (int) (ru.ru_stime.tv_sec * 1000ULL + ru.ru_stime.tv_usec / 1000)},
      {"uptime",    uptimeOfProcess()},
      {"perfstats", perfStatsToJson()}
  };

  resp->setStatusCode(drogon::k200OK);
  resp->setBody(my_json.dump());
  incCommandStat("stats");
  incPrometheusCommandMetric("stats");
}

enum CustomReturnFields {
  customRetStatus = 0, customRetAttrs = 1
};

void parseCustomCmd(const drogon::HttpRequestPtr& req,
                    const std::string& command,
                    const drogon::HttpResponsePtr& resp)
{
  json11::Json msg;
  string err;

  msg = json11::Json::parse(req->bodyData(), err);
  if (msg.is_null()) {
    resp->setStatusCode(drogon::k500InternalServerError);
    std::stringstream ss;
    ss << "{\"success\":\"failure\", \"reason\":\"" << err << "\"}";
    resp->setBody(ss.str());
  }
  else {
    CustomFuncArgs cfa;

    try {
      cfa.setAttrs(msg);
    }
    catch (...) {
      resp->setStatusCode(drogon::k500InternalServerError);
      resp->setBody(R"({"success":false, "reason":"Could not parse input"})");
      return;
    }

    try {
      CustomFuncReturn cr;
      {
        cr = g_luamultip->custom_func(command, cfa);
      }
      bool status = std::get<customRetStatus>(cr);
      KeyValVector ret_attrs = std::get<customRetAttrs>(cr);
      json11::Json::object jattrs;
      for (auto& i : ret_attrs) {
        jattrs.insert(make_pair(i.first, json11::Json(i.second)));
      }
      msg = json11::Json::object{{"success", status},
                         {"r_attrs", jattrs}};

      resp->setStatusCode(drogon::k200OK);
      resp->setBody(msg.dump());
    }
    catch (LuaContext::ExecutionErrorException& e) {
      resp->setStatusCode(drogon::k500InternalServerError);
      try {
        std::rethrow_if_nested(e);
        std::stringstream ss;
        ss << "{\"success\":false, \"reason\":\"" << e.what() << "\"}";
        resp->setBody(ss.str());
        errlog("Lua custom function [%s] exception: %s", command, e.what());
      }
      catch (const std::exception& ne) {
        resp->setStatusCode(drogon::k500InternalServerError);
        std::stringstream ss;
        ss << "{\"success\":false, \"reason\":\"" << ne.what() << "\"}";
        resp->setBody(ss.str());
        errlog("Exception in command [%s] exception: %s", command, ne.what());
      }
      catch (const WforceException& ne) {
        resp->setStatusCode(drogon::k500InternalServerError);
        std::stringstream ss;
        ss << "{\"success\":false, \"reason\":\"" << ne.reason << "\"}";
        resp->setBody(ss.str());
        errlog("Exception in command [%s] exception: %s", command, ne.reason);
      }
    }
    catch (const std::exception& e) {
      resp->setStatusCode(drogon::k500InternalServerError);
      std::stringstream ss;
      ss << "{\"success\":false, \"reason\":\"" << e.what() << "\"}";
      resp->setBody(ss.str());
      errlog("Exception in command [%s] exception: %s", command, e.what());
    }
  }
  incCommandStat(command);
  incPrometheusCommandMetric(command);
}

void registerWebserverCommands()
{
  addCommandStat("report");
  addPrometheusCommandMetric("report");
  g_webserver.registerFunc("report", HTTPVerb::POST, WforceWSFunc(parseReportCmd));
  addCommandStat("stats");
  addPrometheusCommandMetric("stats");
  g_webserver.registerFunc("stats", HTTPVerb::GET, WforceWSFunc(parseStatsCmd));
}
