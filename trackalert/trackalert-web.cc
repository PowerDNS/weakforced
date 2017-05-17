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
#include "trackalert.hh"
#include "ext/json11/json11.hpp"
#include "ext/incbin/incbin.h"
#include "dolog.hh"
#include <chrono>
#include <thread>
#include <sstream>
#include "yahttp/yahttp.hpp"
#include "namespaces.hh"
#include <sys/time.h>
#include <sys/resource.h>
#include "ext/ctpl.h"
#include "base64.hh"
#include "perf-stats.hh"
#include "luastate.hh"
#include "webhook.hh"
#include "login_tuple.hh"

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

void parseReportCmd(const YaHTTP::Request& req, YaHTTP::Response& resp, const std::string& command)
{
  using namespace json11;
  Json msg;
  string err;
  msg=Json::parse(req.body, err);
  if (msg.is_null()) {
    resp.status=500;
    std::stringstream ss;
    ss << "{\"status\":\"failure\", \"reason\":\"" << err << "\"}";
    resp.body=ss.str();
  }
  else {
    try {
      LoginTuple lt;

      lt.from_json(msg, nullptr);
      reportLog(lt);
      g_stats.reports++;
      resp.status=200;
      {
	g_luamultip->report(lt);
      }
      resp.status=200;
      resp.body=R"({"status":"ok"})";
    }
    catch(LuaContext::ExecutionErrorException& e) {
      resp.status=500;
      std::stringstream ss;
      try {
	std::rethrow_if_nested(e);
	ss << "{\"status\":\"failure\", \"reason\":\"" << e.what() << "\"}";
	resp.body=ss.str();
	errlog("Lua function [%s] exception: %s", command, e.what());
      }
      catch (const std::exception& ne) {
	resp.status=500;
	std::stringstream ss;
	ss << "{\"status\":\"failure\", \"reason\":\"" << ne.what() << "\"}";
	resp.body=ss.str();
	errlog("Exception in command [%s] exception: %s", command, ne.what());
      }
      catch (const WforceException& ne) {
	resp.status=500;
	std::stringstream ss;
	ss << "{\"status\":\"failure\", \"reason\":\"" << ne.reason << "\"}";
	resp.body=ss.str();
	errlog("Exception in command [%s] exception: %s", command, ne.reason);
      }
    }
    catch(const std::exception& e) {
      resp.status=500;
      std::stringstream ss;
      ss << "{\"status\":\"failure\", \"reason\":\"" << e.what() << "\"}";
      resp.body=ss.str();
      errlog("Exception in command [%s] exception: %s", command, e.what());
    }
    catch(const WforceException& e) {
      resp.status=500;
      std::stringstream ss;
      ss << "{\"status\":\"failure\", \"reason\":\"" << e.reason << "\"}";
      resp.body=ss.str();
      errlog("Exception in command [%s] exception: %s", command, e.reason);
    }
  }
}

void registerWebserverCommands()
{
  g_webserver.registerFunc("report", HTTPVerb::POST, parseReportCmd);
}
