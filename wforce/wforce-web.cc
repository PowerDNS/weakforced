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

#include "wforce.hh"
#include "sstuff.hh"
#include "json11.hpp"
#include "ext/incbin/incbin.h"
#include "dolog.hh"
#include <chrono>
#include <thread>
#include <sstream>
#include "namespaces.hh"
#include <ctime>
#include <sys/resource.h>
#include "base64.hh"
#include "blackwhitelist.hh"
#include "twmap-wrapper.hh"
#include "customfunc.hh"
#include "perf-stats.hh"
#include "luastate.hh"
#include "login_tuple.hh"
#include "webhook.hh"
#include "boost/date_time/posix_time/posix_time.hpp"
#include "boost/date_time/gregorian/gregorian.hpp"
#include "boost/regex.hpp"
#include "wforce-prometheus.hh"
#include "wforce-sibling.hh"
#include "wforce-replication.hh"

using std::thread;
using namespace boost::posix_time;
using namespace boost::gregorian;

GlobalStateHolder<vector<shared_ptr<Sibling>>> g_report_sinks;
GlobalStateHolder<std::map<std::string, std::pair<std::shared_ptr<std::atomic<unsigned int>>, vector<shared_ptr<Sibling>>>>> g_named_report_sinks;
bool g_builtin_bl_enabled = true;
bool g_builtin_wl_enabled = true;

static time_t start = time(0);

static inline void setErrorCodeAndReason(drogon::HttpStatusCode code, const std::string& reason, const drogon::HttpResponsePtr& resp)
{
  resp->setStatusCode(code);
  json11::Json j = json11::Json::object {
    { "status", "failure" },
    { "reason", reason },
    };
  resp->setBody(j.dump());
}

static int uptimeOfProcess()
{
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
    os << "session_id=\"" << lt.session_id << "\" ";
    os << DeviceAttrsToString(lt);
    os << LtAttrsToString(lt);
    infolog(os.str().c_str());
  }
}

bool g_allowlog_verbose = false;

void allowLog(int retval, const std::string& msg, const LoginTuple& lt,
              const std::vector<pair<std::string, std::string>>& kvs)
{
  std::ostringstream os;
  if ((retval != 0) || g_allowlog_verbose) {
    os << "allowLog " << msg << ": ";
    os << "allow=\"" << retval << "\" ";
    os << "remote=\"" << lt.remote.toString() << "\" ";
    os << "login=\"" << lt.login << "\" ";
    os << "protocol=\"" << lt.protocol << "\" ";
    os << "device_id=\"" << lt.device_id << "\" ";
    os << "session_id=\"" << lt.session_id << "\" ";
    os << DeviceAttrsToString(lt);
    os << LtAttrsToString(lt);
    os << "rattrs={";
    for (const auto& i : kvs) {
      os << i.first << "=" << "\"" << i.second << "\"" << " ";
    }
    os << "}";
  }
  if ((retval == 0) && (!g_allowlog_verbose)) {
    // do not log if retval == 0 and not verbose
  }
  else if ((retval == 0) && g_allowlog_verbose) {
    infolog(os.str().c_str());
  }
  else {
    // only log at notice if login was rejected or tarpitted
    noticelog(os.str().c_str());
  }
}

void addBLWLEntries(const std::vector<BlackWhiteListEntry>& blv, const char* key_name, json11::Json::array& my_entries)
{
  for (auto i = blv.begin(); i != blv.end(); ++i) {
    json11::Json my_entry = json11::Json::object{
        {key_name,     (std::string) i->key},
        {"expiration", (std::string) boost::posix_time::to_simple_string(i->expiration)},
        {"reason",     (std::string) i->reason}
    };
    my_entries.push_back(my_entry);
  }
}

bool canonicalizeLogin(std::string& login, const drogon::HttpResponsePtr& resp)
{
  bool retval = true;

  try {
    // canonicalize the login - e.g. turn "foo" into "foo@foobar.com" and bar into "bar@barfoo.com"
    login = g_luamultip->canonicalize(login);
  }
  catch (LuaContext::ExecutionErrorException& e) {
    setErrorCodeAndReason(drogon::k500InternalServerError, e.what(), resp);
    errlog("Lua canonicalize function exception: %s", e.what());
    retval = false;
  }
  catch (...) {
    setErrorCodeAndReason(drogon::k500InternalServerError, "unknown", resp);
    retval = false;
  }
  return retval;
}

static std::mutex dump_mutex;

void parseDumpEntriesCmd(const drogon::HttpRequestPtr& req,
                         const std::string& command,
                         const drogon::HttpResponsePtr& resp)
{
  json11::Json msg;
  string err;

  resp->setStatusCode(drogon::k200OK);

  msg = json11::Json::parse(req->bodyData(), err);
  if (msg.is_null()) {
    setErrorCodeAndReason(drogon::k400BadRequest, err, resp);
  }
  else {
    try {
      if (msg["dump_host"].is_null() ||
          msg["dump_port"].is_null()) {
        throw WforceException("One of mandatory parameters [dump_host, dump_port] is missing");
      }
      string myip = msg["dump_host"].string_value();
      int myport = msg["dump_port"].int_value();
      ComboAddress replication_ca(myip, myport);
      std::unique_lock<std::mutex> lock(dump_mutex, std::defer_lock);
      if (lock.try_lock()) {
        thread t(dumpEntriesThread, replication_ca, std::move(lock));
        t.detach();
      }
      else {
        setErrorCodeAndReason(drogon::k503ServiceUnavailable, "A DB dump is already in progress", resp);
      }
    }
    catch (const WforceException& e) {
      setErrorCodeAndReason(drogon::k500InternalServerError, e.reason, resp);
      errlog("Exception in command [%s] exception: %s", command, e.reason);
    }
  }
  if (resp->getStatusCode() == drogon::k200OK)
    resp->setBody(R"({"status":"ok"})");
  incCommandStat("dumpEntries");
  incPrometheusCommandMetric("dumpEntries");

}

void parseSyncCmd(const drogon::HttpRequestPtr& req,
                  const std::string& command,
                  const drogon::HttpResponsePtr& resp)
{
  json11::Json msg;
  string err;

  resp->setStatusCode(drogon::k200OK);

  msg = json11::Json::parse(req->bodyData(), err);
  if (msg.is_null()) {
    setErrorCodeAndReason(drogon::k400BadRequest, err, resp);
  }
  else {
    try {
      if (msg["replication_host"].is_null() ||
          msg["replication_port"].is_null() ||
          msg["callback_url"].is_null() ||
          msg["callback_auth_pw"].is_null()) {
        throw WforceException(
            "One of mandatory parameters [replication_host, replication_port, callback_url, callback_auth_pw] is missing");
      }
      string myip = msg["replication_host"].string_value();
      int myport = msg["replication_port"].int_value();
      ComboAddress replication_ca(myip, myport);
      std::string callback_url = msg["callback_url"].string_value();
      std::string callback_pw = msg["callback_auth_pw"].string_value();
      std::string encryption_key;
      if (!msg["encryption_key"].is_null()) {
        encryption_key = msg["encryption_key"].string_value();
      }
      else {
        encryption_key = g_replication.getEncryptionKey();
      }
      thread t(syncDBThread, replication_ca, callback_url, callback_pw, encryption_key);
      t.detach();
    }
    catch (const WforceException& e) {
      setErrorCodeAndReason(drogon::k500InternalServerError, e.reason, resp);
      errlog("Exception in command [%s] exception: %s", command, e.reason);
    }
  }
  if (resp->getStatusCode() == drogon::k200OK)
    resp->setBody(R"({"status":"ok"})");
  incCommandStat("syncDBs");
  incPrometheusCommandMetric("syncDBs");
}

void parseAddSiblingCmd(const drogon::HttpRequestPtr& req,
                        const std::string& command,
                        const drogon::HttpResponsePtr& resp)
{
  json11::Json msg;
  string err;

  resp->setStatusCode(drogon::k200OK);

  msg = json11::Json::parse(req->bodyData(), err);
  if (msg.is_null()) {
    setErrorCodeAndReason(drogon::k400BadRequest, err, resp);
  }
  else {
    try {
      if (msg["sibling_host"].is_null() ||
          msg["sibling_port"].is_null()) {
        throw WforceException("One of mandatory parameters [sibling_host, sibling_port] is missing");
      }
      std::string sibling_host = msg["sibling_host"].string_value();
      int sibling_port = msg["sibling_port"].int_value();
      bool has_encryption_key = false;
      std::string encryption_key;
      if (!msg["encryption_key"].is_null()) {
        has_encryption_key = true;
        encryption_key = msg["encryption_key"].string_value();
      }

      Sibling::Protocol proto = Sibling::Protocol::UDP;
      if (!msg["sibling_protocol"].is_null()) {
        proto = Sibling::stringToProtocol(msg["sibling_protocol"].string_value());
      }
      if (proto == Sibling::Protocol::NONE) {
        throw WforceException(std::string("Invalid sibling_protocol: ") + msg["sibling_protocol"].string_value());
      }

      std::string error_msg;
      if (!has_encryption_key) {
        if (!addSibling(sibling_host, sibling_port, proto, g_replication.getSiblings(), error_msg)) {
          throw WforceException(error_msg);
        }
      }
      else {
        if (!addSiblingWithKey(sibling_host, sibling_port, proto, g_replication.getSiblings(), error_msg,
                               encryption_key)) {
          throw WforceException(error_msg);
        }
      }
    }
    catch (const WforceException& e) {
      setErrorCodeAndReason(drogon::k500InternalServerError, e.reason, resp);
      errlog("Exception in command [%s] exception: %s", command, e.reason);
    }
  }
  if (resp->getStatusCode() == drogon::k200OK)
    resp->setBody(R"({"status":"ok"})");
  incCommandStat("addSibling");
  incPrometheusCommandMetric("addSibling");
}

void parseRemoveSiblingCmd(const drogon::HttpRequestPtr& req,
                           const std::string& command,
                           const drogon::HttpResponsePtr& resp)
{
  json11::Json msg;
  string err;

  resp->setStatusCode(drogon::k200OK);

  msg = json11::Json::parse(req->bodyData(), err);
  if (msg.is_null()) {
    setErrorCodeAndReason(drogon::k400BadRequest, err, resp);
  }
  else {
    try {
      if (msg["sibling_host"].is_null() ||
          msg["sibling_port"].is_null()) {
        throw WforceException("One of mandatory parameters [sibling_host, sibling_port] is missing");
      }
      std::string sibling_host = msg["sibling_host"].string_value();
      int sibling_port = msg["sibling_port"].int_value();

      std::string error_msg;
      if (!removeSibling(sibling_host, sibling_port, g_replication.getSiblings(), error_msg)) {
        throw WforceException(error_msg);
      }
    }
    catch (const WforceException& e) {
      setErrorCodeAndReason(drogon::k500InternalServerError, e.reason, resp);
      errlog("Exception in command [%s] exception: %s", command, e.reason);
    }
  }
  if (resp->getStatusCode() == drogon::k200OK)
    resp->setBody(R"({"status":"ok"})");
  incCommandStat("removeSibling");
  incPrometheusCommandMetric("removeSibling");
}

void parseSetSiblingsCmd(const drogon::HttpRequestPtr& req,
                         const std::string& command,
                         const drogon::HttpResponsePtr& resp)
{
  json11::Json msg;
  string err;
  std::vector<std::pair<int, std::vector<std::pair<int, std::string>>>> new_siblings;

  resp->setStatusCode(drogon::k200OK);

  msg = json11::Json::parse(req->bodyData(), err);
  if (msg.is_null()) {
    setErrorCodeAndReason(drogon::k400BadRequest, err, resp);
  }
  else {
    try {
      if (msg["siblings"].is_null() || !msg["siblings"].is_array()) {
        throw WforceException("Mandatory parameter [siblings] is missing or is not an array");
      }
      auto siblings = msg["siblings"].array_items();
      for (auto& i : siblings) {
        if (i["sibling_host"].is_null() || i["sibling_port"].is_null()) {
          throw WforceException("Mandatory parameter(s) [sibling_host, sibling_port] are missing");
        }
        std::string sibling_host = i["sibling_host"].string_value();
        int sibling_port = i["sibling_port"].int_value();
        std::string encryption_key;
        if (!i["encryption_key"].is_null()) {
          encryption_key = i["encryption_key"].string_value();
        }
        else {
          encryption_key = Base64Encode(g_replication.getEncryptionKey());
        }

        Sibling::Protocol proto = Sibling::Protocol::UDP;
        if (!i["sibling_protocol"].is_null()) {
          proto = Sibling::stringToProtocol(i["sibling_protocol"].string_value());
        }
        if (proto == Sibling::Protocol::NONE) {
          throw WforceException(std::string("Invalid sibling_protocol: ") + msg["sibling_protocol"].string_value());
        }
        new_siblings.emplace_back(std::pair<int, std::vector<std::pair<int, std::string>>>{0, {{0, createSiblingAddress(
            sibling_host, sibling_port, proto)}, {1, encryption_key}}});
      }
      std::string error_msg;
      if (!setSiblingsWithKey(new_siblings, g_replication.getSiblings(), error_msg)) {
        throw WforceException(error_msg);
      }
    }
    catch (const WforceException& e) {
      setErrorCodeAndReason(drogon::k500InternalServerError, e.reason, resp);
      errlog("Exception in command [%s] exception: %s", command, e.reason);
    }
  }
  if (resp->getStatusCode() == drogon::k200OK)
    resp->setBody(R"({"status":"ok"})");
  incCommandStat("setSiblings");
  incPrometheusCommandMetric("setSiblings");
}

void parseAddDelBLWLEntryCmd(const drogon::HttpRequestPtr& req,
                             const drogon::HttpResponsePtr& resp,
                             bool addCmd, bool blacklist)
{
  json11::Json msg;
  string err;

  resp->setStatusCode(drogon::k200OK);

  msg = json11::Json::parse(req->bodyData(), err);
  if (msg.is_null()) {
    setErrorCodeAndReason(drogon::k400BadRequest, err, resp);
  }
  else {
    bool haveIP = false;
    bool haveLogin = false;
    bool haveNetmask = false;
    unsigned int bl_seconds = 0;
    std::string bl_reason;
    std::string en_login;
    ComboAddress en_ca;
    Netmask en_nm;

    try {
      if (!msg["ip"].is_null()) {
        string myip = msg["ip"].string_value();
        en_ca = ComboAddress(myip);
        en_nm = Netmask(myip);
        haveIP = true;
      }
      if (!msg["netmask"].is_null()) {
        string mynm = msg["netmask"].string_value();
        en_nm = Netmask(mynm);
        haveNetmask = true;
      }
      if (!msg["login"].is_null()) {
        en_login = msg["login"].string_value();
        if (!canonicalizeLogin(en_login, resp))
          return;
        haveLogin = true;
      }
      if (addCmd) {
        if (!msg["expire_secs"].is_null()) {
          bl_seconds = msg["expire_secs"].int_value();
        }
        else {
          throw std::runtime_error("Missing mandatory expire_secs field");
        }
        if (!msg["reason"].is_null()) {
          bl_reason = msg["reason"].string_value();
        }
        else {
          throw std::runtime_error("Missing mandatory reason field");
        }
      }
      if (haveLogin && haveIP) {
        if (addCmd) {
          if (blacklist)
            g_bl_db.addEntry(en_ca, en_login, bl_seconds, bl_reason);
          else
            g_wl_db.addEntry(en_ca, en_login, bl_seconds, bl_reason);
        }
        else {
          if (blacklist)
            g_bl_db.deleteEntry(en_ca, en_login);
          else
            g_wl_db.deleteEntry(en_ca, en_login);
        }
      }
      else if (haveLogin) {
        if (addCmd) {
          if (blacklist)
            g_bl_db.addEntry(en_login, bl_seconds, bl_reason);
          else
            g_wl_db.addEntry(en_login, bl_seconds, bl_reason);
        }
        else {
          if (blacklist)
            g_bl_db.deleteEntry(en_login);
          else
            g_wl_db.deleteEntry(en_login);
        }
      }
      else if (haveIP || haveNetmask) {
        if (addCmd) {
          if (blacklist)
            g_bl_db.addEntry(en_nm, bl_seconds, bl_reason);
          else
            g_wl_db.addEntry(en_nm, bl_seconds, bl_reason);
        }
        else {
          if (blacklist)
            g_bl_db.deleteEntry(en_nm);
          else
            g_wl_db.deleteEntry(en_nm);
        }
      }
    }
    catch (std::runtime_error& e) {
      setErrorCodeAndReason(drogon::k500InternalServerError, e.what(), resp);
    }
    catch (...) {
      resp->setStatusCode(drogon::k500InternalServerError);
      resp->setBody(R"({"status":"failure"})");
    }
  }
  if (resp->getStatusCode() == drogon::k200OK)
    resp->setBody(R"({"status":"ok"})");
}

void parseAddBLEntryCmd(const drogon::HttpRequestPtr& req,
                        const std::string& command,
                        const drogon::HttpResponsePtr& resp)
{
  parseAddDelBLWLEntryCmd(req, resp, true, true);
  incCommandStat("addBLEntry");
  incPrometheusCommandMetric("addBLEntry");
}

void parseDelBLEntryCmd(const drogon::HttpRequestPtr& req,
                        const std::string& command,
                        const drogon::HttpResponsePtr& resp)
{
  parseAddDelBLWLEntryCmd(req, resp, false, true);
  incCommandStat("delBLEntry");
  incPrometheusCommandMetric("delBLEntry");
}

void parseAddWLEntryCmd(const drogon::HttpRequestPtr& req,
                        const std::string& command,
                        const drogon::HttpResponsePtr& resp)
{
  parseAddDelBLWLEntryCmd(req, resp, true, false);
  incCommandStat("addWLEntry");
  incPrometheusCommandMetric("addWLEntry");
}

void parseDelWLEntryCmd(const drogon::HttpRequestPtr& req,
                        const std::string& command,
                        const drogon::HttpResponsePtr& resp)
{
  parseAddDelBLWLEntryCmd(req, resp, false, false);
  incCommandStat("delWLEntry");
  incPrometheusCommandMetric("delWLEntry");
}

void parseResetCmd(const drogon::HttpRequestPtr& req,
                   const std::string& command,
                   const drogon::HttpResponsePtr& resp)
{
  json11::Json msg;
  string err;
  msg = json11::Json::parse(req->bodyData(), err);
  if (msg.is_null()) {
    setErrorCodeAndReason(drogon::k400BadRequest, err, resp);
  }
  else {
    try {
      bool haveIP = false;
      bool haveLogin = false;
      std::string en_type, en_login;
      ComboAddress en_ca;
      if (!msg["ip"].is_null()) {
        en_ca = ComboAddress(msg["ip"].string_value());
        haveIP = true;
      }
      if (!msg["login"].is_null()) {
        en_login = msg["login"].string_value();
        // canonicalize the login - e.g. turn "foo" into "foo@foobar.com" and bar into "bar@barfoo.com"
        if (!canonicalizeLogin(en_login, resp))
          return;
        haveLogin = true;
      }
      if (haveLogin && haveIP) {
        en_type = "ip:login";
        g_bl_db.deleteEntry(en_ca, en_login);
      }
      else if (haveLogin) {
        en_type = "login";
        g_bl_db.deleteEntry(en_login);
      }
      else if (haveIP) {
        en_type = "ip";
        g_bl_db.deleteEntry(en_ca);
      }

      if (!haveLogin && !haveIP) {
        resp->setStatusCode(drogon::k400BadRequest);
        resp->setBody(R"({"status":"failure", "reason":"No ip or login field supplied"})");
      }
      else {
        bool reset_ret;
        {
          reset_ret = g_luamultip->reset(en_type, en_login, en_ca);
        }
        if (reset_ret) {
          resp->setStatusCode(drogon::k200OK);
          resp->setBody(R"({"status":"ok"})");
        }
        else {
          resp->setStatusCode(drogon::k500InternalServerError);
          resp->setBody(R"({"status":"failure", "reason":"reset function returned false"})");
        }
      }
      // generate webhook events
      json11::Json jobj = json11::Json::object{{"login", en_login},
                                               {"ip",    en_ca.toString()},
                                               {"type",  "wforce_reset"}};
      for (const auto& h : g_webhook_db.getWebHooksForEvent("reset")) {
        if (auto hs = h.lock()) {
          g_webhook_runner.runHook("reset", hs, jobj);
        }
      }
    }
    catch (LuaContext::ExecutionErrorException& e) {
      setErrorCodeAndReason(drogon::k500InternalServerError, e.what(), resp);
      errlog("Lua reset function exception: %s", e.what());
    }
    catch (...) {
      resp->setStatusCode(drogon::k500InternalServerError);
      resp->setBody(R"({"status":"failure"})");
    }
  }
  incCommandStat("reset");
  incPrometheusCommandMetric("reset");
}

void parseReportCmd(const drogon::HttpRequestPtr& req,
                    const std::string& command,
                    const drogon::HttpResponsePtr& resp)
{
  json11::Json msg;
  string err;
  msg = json11::Json::parse(req->bodyData(), err);
  if (msg.is_null()) {
    setErrorCodeAndReason(drogon::k400BadRequest, err, resp);
  }
  else {
    try {
      LoginTuple lt;

      lt.from_json(msg, g_ua_parser_p);

      // canonicalize the login - e.g. turn "foo" into "foo@foobar.com" and bar into "bar@barfoo.com"
      if (!canonicalizeLogin(lt.login, resp))
        return;
      lt.t = getDoubleTime(); // set the time
      reportLog(lt);
      g_stats.reports++;
      {
        g_luamultip->report(lt);
      }

      // If any normal or named report sinks are configured, send the report to one of them
      sendReportSink(lt); // XXX - this is deprecated now in favour of NamedReportSinks
      sendNamedReportSink(lt.serialize());

      json11::Json msg = lt.to_json();
      json11::Json::object jobj = msg.object_items();
      jobj.insert(make_pair("type", "wforce_report"));
      for (const auto& h : g_webhook_db.getWebHooksForEvent("report")) {
        if (auto hs = h.lock())
          g_webhook_runner.runHook("report", hs, jobj);
      }

      resp->setStatusCode(drogon::k200OK);
      resp->setBody(R"({"status":"ok"})");
    }
    catch (LuaContext::ExecutionErrorException& e) {
      resp->setStatusCode(drogon::k500InternalServerError);
      std::stringstream ss;
      try {
        std::rethrow_if_nested(e);
        setErrorCodeAndReason(drogon::k500InternalServerError, e.what(), resp);
        errlog("Lua function [%s] exception: %s", command, e.what());
      }
      catch (const std::exception& ne) {
        setErrorCodeAndReason(drogon::k500InternalServerError, ne.what(), resp);
        errlog("Exception in command [%s] exception: %s", command, ne.what());
      }
      catch (const WforceException& ne) {
        setErrorCodeAndReason(drogon::k500InternalServerError, ne.reason, resp);
        errlog("Exception in command [%s] exception: %s", command, ne.reason);
      }
    }
    catch (const std::exception& e) {
      setErrorCodeAndReason(drogon::k500InternalServerError, e.what(), resp);
      errlog("Exception in command [%s] exception: %s", command, e.what());
    }
    catch (const WforceException& e) {
      setErrorCodeAndReason(drogon::k500InternalServerError, e.reason, resp);
      errlog("Exception in command [%s] exception: %s", command, e.reason);
    }
  }
  incCommandStat("report");
  incPrometheusCommandMetric("report");
}

#define OX_PROTECT_NOTIFY 0

void genProtectHookData(const json11::Json& lt, const std::string& msg, json11::Json& ret_json)
{
  json11::Json::object jobj;

  double dtime = lt["t"].number_value();
  unsigned int isecs = (int) dtime;
  unsigned int iusecs = (dtime - isecs) * 1000000;

  ptime t(date(1970, Jan, 01), seconds(isecs) + microsec(iusecs));
  jobj.insert(std::make_pair("timestamp", to_iso_extended_string(t) + "Z"));

  jobj.insert(std::make_pair("user", lt["login"].string_value()));
  jobj.insert(std::make_pair("device", lt["device_id"].string_value()));
  jobj.insert(std::make_pair("ip", lt["remote"].string_value()));
  jobj.insert(std::make_pair("message", msg));
  jobj.insert(std::make_pair("code", OX_PROTECT_NOTIFY));
  jobj.insert(std::make_pair("application", "wforce"));

  ret_json = json11::Json(std::move(jobj));
}

bool allow_filter(std::shared_ptr<const WebHook> hook, int status)
{
  bool retval = false;
  if (hook->hasConfigKey("allow_filter")) {
    std::string filter = hook->getConfigKey("allow_filter");
    if (((filter.find("reject") != string::npos) && (status < 0)) ||
        (((filter.find("allow") != string::npos) && (status == 0))) ||
        (((filter.find("tarpit") != string::npos) && (status > 0)))) {
      retval = true;
      vdebuglog("allow_filter: filter evaluates to true (allowing event)");
    }
  }
  else
    retval = true;

  return retval;
}

void runAllowWebHook(const json11::Json& request, const json11::Json& response, int status)
{
  json11::Json jobj = json11::Json::object{{"request",  request},
                                           {"response", response},
                                           {"type",     "wforce_allow"}};
  for (const auto& h : g_webhook_db.getWebHooksForEvent("allow")) {
    if (auto hs = h.lock()) {
      json11::Json pj;
      if (hs->hasConfigKey("ox-protect")) {
        genProtectHookData(jobj["request"],
                           hs->getConfigKey("ox-protect"),
                           pj);
        if (allow_filter(hs, status))
          g_webhook_runner.runHook("allow", hs, pj);
      }
      else {
        if (allow_filter(hs, status))
          g_webhook_runner.runHook("allow", hs, jobj);
      }
    }
  }
}

enum AllowReturnFields {
  allowRetStatus = 0, allowRetMsg = 1, allowRetLogMsg = 2, allowRetAttrs = 3
};

void parseAllowCmd(const drogon::HttpRequestPtr& req,
                   const std::string& command,
                   const drogon::HttpResponsePtr& resp)
{
  json11::Json msg;
  string err;
  std::vector<pair<std::string, std::string>> ret_attrs;

  incCommandStat("allow");
  incPrometheusCommandMetric("allow");
  g_stats.allows++;

  resp->setStatusCode(drogon::k200OK);

  msg = json11::Json::parse(req->bodyData(), err);
  if (msg.is_null()) {
    setErrorCodeAndReason(drogon::k400BadRequest, err, resp);
  }
  else {
    LoginTuple lt;
    int status = -1;
    std::string ret_msg;

    try {
      lt.from_json(msg, g_ua_parser_p);
      lt.t = getDoubleTime();
    }
    catch (const std::exception& e) {
      setErrorCodeAndReason(drogon::k500InternalServerError, e.what(), resp);
      errlog("Exception in command [%s] exception: %s", command, e.what());
      return;
    }
    catch (const WforceException& e) {
      setErrorCodeAndReason(drogon::k500InternalServerError, e.reason, resp);
      errlog("Exception in command [%s] exception: %s", command, e.reason);
      return;
    }

    if (!canonicalizeLogin(lt.login, resp)) {
      return;
    }

    // check the built-in whitelists
    bool whitelisted = false;
    BlackWhiteListEntry wle;
    if (g_builtin_wl_enabled) {
      if (g_wl_db.getEntry(lt.remote, wle)) {
        std::vector<pair<std::string, std::string>> log_attrs =
            {{"expiration",  boost::posix_time::to_simple_string(wle.expiration)},
             {"reason",      wle.reason},
             {"whitelisted", "1"},
             {"key",         "ip"}};
        status = 0;
        allowLog(status, std::string("whitelisted IP"), lt, log_attrs);
        ret_msg = g_wl_db.getIPRetMsg();
        ret_attrs = std::move(log_attrs);
        whitelisted = true;
      }
      else if (g_wl_db.getEntry(lt.login, wle)) {
        std::vector<pair<std::string, std::string>> log_attrs =
            {{"expiration",  boost::posix_time::to_simple_string(wle.expiration)},
             {"reason",      wle.reason},
             {"whitelisted", "1"},
             {"key",         "login"}};
        status = 0;
        allowLog(status, std::string("whitelisted Login"), lt, log_attrs);
        ret_msg = g_wl_db.getLoginRetMsg();
        ret_attrs = std::move(log_attrs);
        whitelisted = true;
      }
      else if (g_wl_db.getEntry(lt.remote, lt.login, wle)) {
        std::vector<pair<std::string, std::string>> log_attrs =
            {{"expiration",  boost::posix_time::to_simple_string(wle.expiration)},
             {"reason",      wle.reason},
             {"whitelisted", "1"},
             {"key",         "iplogin"}};
        status = 0;
        allowLog(status, std::string("whitelisted IPLogin"), lt, log_attrs);
        ret_msg = g_wl_db.getIPLoginRetMsg();
        ret_attrs = std::move(log_attrs);
        whitelisted = true;
      }
    }

    // next check the built-in blacklists
    bool blacklisted = false;
    BlackWhiteListEntry ble;
    if (g_builtin_bl_enabled) {
      if (g_bl_db.getEntry(lt.remote, ble)) {
        std::vector<pair<std::string, std::string>> log_attrs =
            {{"expiration",  boost::posix_time::to_simple_string(ble.expiration)},
             {"reason",      ble.reason},
             {"blacklisted", "1"},
             {"key",         "ip"}};
        allowLog(status, std::string("blacklisted IP"), lt, log_attrs);
        ret_msg = g_bl_db.getIPRetMsg();
        ret_attrs = std::move(log_attrs);
        blacklisted = true;
      }
      else if (g_bl_db.getEntry(lt.login, ble)) {
        std::vector<pair<std::string, std::string>> log_attrs =
            {{"expiration",  boost::posix_time::to_simple_string(ble.expiration)},
             {"reason",      ble.reason},
             {"blacklisted", "1"},
             {"key",         "login"}};
        allowLog(status, std::string("blacklisted Login"), lt, log_attrs);
        ret_msg = g_bl_db.getLoginRetMsg();
        ret_attrs = std::move(log_attrs);
        blacklisted = true;
      }
      else if (g_bl_db.getEntry(lt.remote, lt.login, ble)) {
        std::vector<pair<std::string, std::string>> log_attrs =
            {{"expiration",  boost::posix_time::to_simple_string(ble.expiration)},
             {"reason",      ble.reason},
             {"blacklisted", "1"},
             {"key",         "iplogin"}};
        allowLog(status, std::string("blacklisted IPLogin"), lt, log_attrs);
        ret_msg = g_bl_db.getIPLoginRetMsg();
        ret_attrs = std::move(log_attrs);
        blacklisted = true;
      }
    }

    if (blacklisted || whitelisted) {
      // run webhook
      json11::Json::object jattrs;
      for (auto& i : ret_attrs) {
        jattrs.insert(make_pair(i.first, json11::Json(i.second)));
      }
      msg = json11::Json::object{{"status",  status},
                                 {"msg",     ret_msg},
                                 {"r_attrs", jattrs}};
      runAllowWebHook(lt.to_json(), msg, status);
      if (blacklisted) {
        g_stats.denieds++;
        incCommandStat("allow_blacklisted");
        incPrometheusAllowStatusMetric("blacklisted");
        incCommandStat("allow_denied");
        incPrometheusAllowStatusMetric("denied");
      }
      if (whitelisted) {
        incCommandStat("allow_whitelisted");
        incPrometheusAllowStatusMetric("whitelisted");
      }
    }
    else {
      try {
        AllowReturn ar;
        {
          ar = g_luamultip->allow(lt);
        }
        status = std::get<allowRetStatus>(ar);
        ret_msg = std::get<allowRetMsg>(ar);
        std::string log_msg = std::get<allowRetLogMsg>(ar);
        std::vector<pair<std::string, std::string>> log_attrs = std::get<allowRetAttrs>(ar);

        // log the results of the allow function
        allowLog(status, log_msg, lt, log_attrs);

        ret_attrs = std::move(log_attrs);
        if (status < 0) {
          g_stats.denieds++;
          incCommandStat("allow_denied");
          incPrometheusAllowStatusMetric("denied");
        }
        else if (status > 0) {
          incCommandStat("allow_tarpitted");
          incPrometheusAllowStatusMetric("tarpitted");
        }
        else {
          incCommandStat("allow_allowed");
          incPrometheusAllowStatusMetric("allowed");
        }
        json11::Json::object jattrs;
        for (auto& i : ret_attrs) {
          jattrs.insert(make_pair(i.first, json11::Json(i.second)));
        }
        msg = json11::Json::object{{"status",  status},
                                   {"msg",     ret_msg},
                                   {"r_attrs", jattrs}};

        // generate webhook events
        runAllowWebHook(lt.to_json(), msg, status);

        resp->setStatusCode(drogon::k200OK);
        resp->setBody(msg.dump());
      }
      catch (LuaContext::ExecutionErrorException& e) {
        resp->setStatusCode(drogon::k500InternalServerError);
        std::stringstream ss;
        try {
          std::rethrow_if_nested(e);
          setErrorCodeAndReason(drogon::k500InternalServerError, e.what(), resp);
          errlog("Lua function [%s] exception: %s", command, e.what());
        }
        catch (const std::exception& ne) {
          setErrorCodeAndReason(drogon::k500InternalServerError, ne.what(), resp);
          errlog("Exception in command [%s] exception: %s", command, ne.what());
        }
        catch (const WforceException& ne) {
          setErrorCodeAndReason(drogon::k500InternalServerError, ne.reason, resp);
          errlog("Exception in command [%s] exception: %s", command, ne.reason);
        }
      }
      catch (const std::exception& e) {
        setErrorCodeAndReason(drogon::k500InternalServerError, e.what(), resp);
        errlog("Exception in command [%s] exception: %s", command, e.what());
      }
      catch (const WforceException& e) {
        setErrorCodeAndReason(drogon::k500InternalServerError, e.reason, resp);
        errlog("Exception in command [%s] exception: %s", command, e.reason);
      }
    }
    if (resp->getStatusCode() == drogon::k200OK) {
      json11::Json::object jattrs;
      for (auto& i : ret_attrs) {
        jattrs.insert(make_pair(i.first, json11::Json(i.second)));
      }
      msg = json11::Json::object{{"status",  status},
                                 {"msg",     ret_msg},
                                 {"r_attrs", jattrs}};
      resp->setBody(msg.dump());
    }
  }
}

// XXX BEGIN Deprecated functions - will be removed in a later release
void parseStatsCmd(const drogon::HttpRequestPtr& req,
                   const std::string& command,
                   const drogon::HttpResponsePtr& resp)
{
  struct rusage ru;
  getrusage(RUSAGE_SELF, &ru);

  json11::Json my_json = json11::Json::object{
      {"reports",      (int) g_stats.reports},
      {"allows",       (int) g_stats.allows},
      {"denieds",      (int) g_stats.denieds},
      {"user-msec",    (int) (ru.ru_utime.tv_sec * 1000ULL + ru.ru_utime.tv_usec / 1000)},
      {"sys-msec",     (int) (ru.ru_stime.tv_sec * 1000ULL + ru.ru_stime.tv_usec / 1000)},
      {"uptime",       uptimeOfProcess()},
      {"perfstats",    perfStatsToJson()},
      {"commandstats", commandStatsToJson()},
      {"customstats",  customStatsToJson()}
  };

  resp->setStatusCode(drogon::k200OK);
  resp->setBody(my_json.dump());
  incCommandStat("stats");
  incPrometheusCommandMetric("stats");
}
// XXX END Deprecated functions - will be removed in a later release

void parseGetBLWLCmd(const drogon::HttpRequestPtr& req,
                     const std::string& command,
                     const drogon::HttpResponsePtr& resp, bool blacklist)
{
  json11::Json::array my_entries;
  std::vector<BlackWhiteListEntry> lv;
  std::string key_name;
  std::string cmd_stat;

  if (blacklist) {
    key_name = "bl_entries";
    cmd_stat = "getBL";
  }
  else {
    key_name = "wl_entries";
    cmd_stat = "getWL";
  }

  if (blacklist)
    lv = g_bl_db.getIPEntries();
  else
    lv = g_wl_db.getIPEntries();
  addBLWLEntries(lv, "ip", my_entries);
  if (blacklist)
    lv = g_bl_db.getLoginEntries();
  else
    lv = g_wl_db.getLoginEntries();
  addBLWLEntries(lv, "login", my_entries);
  if (blacklist)
    lv = g_bl_db.getIPLoginEntries();
  else
    lv = g_wl_db.getIPLoginEntries();
  addBLWLEntries(lv, "iplogin", my_entries);

  json11::Json ret_json = json11::Json::object{
      {key_name, my_entries}
  };
  resp->setStatusCode(drogon::k200OK);
  resp->setBody(ret_json.dump());
  incCommandStat(cmd_stat);
  incPrometheusCommandMetric(cmd_stat);
}

void parseGetBLCmd(const drogon::HttpRequestPtr& req,
                   const std::string& command,
                   const drogon::HttpResponsePtr& resp)
{
  parseGetBLWLCmd(req, command, resp, true);
}

void parseGetWLCmd(const drogon::HttpRequestPtr& req,
                   const std::string& command,
                   const drogon::HttpResponsePtr& resp)
{
  parseGetBLWLCmd(req, command, resp, false);
}

void parseGetStatsCmd(const drogon::HttpRequestPtr& req,
                      const std::string& command,
                      const drogon::HttpResponsePtr& resp)
{
  json11::Json msg;
  string err;
  msg = json11::Json::parse(req->bodyData(), err);
  if (msg.is_null()) {
    setErrorCodeAndReason(drogon::k400BadRequest, err, resp);
  }
  else {
    bool haveIP = false;
    bool haveLogin = false;
    std::string en_type, en_login;
    std::string key_name, key_value;
    TWKeyType lookup_key;
    bool is_blacklisted = false;
    bool is_whitelisted = false;
    std::string bl_reason;
    std::string bl_expire;
    std::string wl_reason;
    std::string wl_expire;
    ComboAddress en_ca;
    BlackWhiteListEntry bwle;

    try {
      if (!msg["ip"].is_null()) {
        string myip = msg["ip"].string_value();
        en_ca = ComboAddress(myip);
        haveIP = true;
      }
      if (!msg["login"].is_null()) {
        en_login = msg["login"].string_value();
        if (!canonicalizeLogin(en_login, resp))
          return;
        haveLogin = true;
      }
      if (haveLogin && haveIP) {
        key_name = "ip_login";
        key_value = en_ca.toString() + ":" + en_login;
        lookup_key = key_value;
        if (g_bl_db.getEntry(en_ca, en_login, bwle)) {
          is_blacklisted = true;
          bl_reason = bwle.reason;
          bl_expire = boost::posix_time::to_simple_string(bwle.expiration);
        }
        if (g_wl_db.getEntry(en_ca, en_login, bwle)) {
          is_whitelisted = true;
          wl_reason = bwle.reason;
          wl_expire = boost::posix_time::to_simple_string(bwle.expiration);
        }
      }
      else if (haveLogin) {
        key_name = "login";
        key_value = en_login;
        lookup_key = en_login;
        if (g_bl_db.getEntry(en_login, bwle)) {
          is_blacklisted = true;
          bl_reason = bwle.reason;
          bl_expire = boost::posix_time::to_simple_string(bwle.expiration);
        }
        if (g_wl_db.getEntry(en_login, bwle)) {
          is_whitelisted = true;
          wl_reason = bwle.reason;
          wl_expire = boost::posix_time::to_simple_string(bwle.expiration);
        }
      }
      else if (haveIP) {
        key_name = "ip";
        key_value = en_ca.toString();
        lookup_key = en_ca;
        if (g_bl_db.getEntry(en_ca, bwle)) {
          is_blacklisted = true;
          bl_reason = bwle.reason;
          bl_expire = boost::posix_time::to_simple_string(bwle.expiration);
        }
        if (g_wl_db.getEntry(en_ca, bwle)) {
          is_whitelisted = true;
          wl_reason = bwle.reason;
          wl_expire = boost::posix_time::to_simple_string(bwle.expiration);
        }
      }
    }
    catch (...) {
      setErrorCodeAndReason(drogon::k400BadRequest, "Could not parse input", resp);
      return;
    }

    if (!haveLogin && !haveIP) {
      setErrorCodeAndReason(drogon::k400BadRequest, "No ip or login field supplied", resp);
    }
    else {
      json11::Json::object js_db_stats;
      {
        std::lock_guard<std::mutex> lock(dbMap_mutx);
        for (auto i = dbMap.begin(); i != dbMap.end(); ++i) {
          std::string dbName = i->first;
          std::vector<std::pair<std::string, int>> db_fields;
          if (i->second.get_all_fields(lookup_key, db_fields)) {
            json11::Json::object js_db_fields;
            for (auto j = db_fields.begin(); j != db_fields.end(); ++j) {
              js_db_fields.insert(make_pair(j->first, j->second));
            }
            js_db_stats.insert(make_pair(dbName, js_db_fields));
          }
        }
      }
      json11::Json ret_json = json11::Json::object{
          {key_name,      key_value},
          {"whitelisted", is_whitelisted},
          {"wl_reason",   wl_reason},
          {"wl_expire",   wl_expire},
          {"blacklisted", is_blacklisted},
          {"bl_reason",   bl_reason},
          {"bl_expire",   bl_expire},
          {"stats",       js_db_stats}
      };
      resp->setStatusCode(drogon::k200OK);
      resp->setBody(ret_json.dump());
    }
  }
  incCommandStat("getDBStats");
  incPrometheusCommandMetric("getDBStats");
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
  KeyValVector ret_attrs;

  msg = json11::Json::parse(req->bodyData(), err);
  if (msg.is_null()) {
    setErrorCodeAndReason(drogon::k400BadRequest, err, resp);
  }
  else {
    CustomFuncArgs cfa;
    bool status = false;

    try {
      cfa.setAttrs(msg);
    }
    catch (...) {
      setErrorCodeAndReason(drogon::k400BadRequest, "Could not parse input", resp);
      return;
    }

    try {
      bool reportSink = false;
      CustomFuncReturn cr;
      {
        cr = g_luamultip->custom_func(command, cfa, reportSink);
      }
      status = std::get<customRetStatus>(cr);
      ret_attrs = std::get<customRetAttrs>(cr);
      json11::Json::object jattrs;
      for (auto& i : ret_attrs) {
        jattrs.insert(make_pair(i.first, json11::Json(i.second)));
      }
      msg = json11::Json::object{{"success", status},
                                 {"r_attrs", jattrs}};

      // send the custom parameters to the report sink if configured
      if (reportSink)
        sendNamedReportSink(cfa.serialize());

      resp->setStatusCode(drogon::k200OK);
      resp->setBody(msg.dump());
    }
    catch (LuaContext::ExecutionErrorException& e) {
      resp->setStatusCode(drogon::k500InternalServerError);
      std::stringstream ss;
      try {
        std::rethrow_if_nested(e);
        setErrorCodeAndReason(drogon::k500InternalServerError, e.what(), resp);
        errlog("Lua custom function [%s] exception: %s", command, e.what());
      }
      catch (const std::exception& ne) {
        setErrorCodeAndReason(drogon::k500InternalServerError, ne.what(), resp);
        errlog("Exception in command [%s] exception: %s", command, ne.what());
      }
      catch (const WforceException& ne) {
        setErrorCodeAndReason(drogon::k500InternalServerError, ne.reason, resp);
        errlog("Exception in command [%s] exception: %s", command, ne.reason);
      }
    }
    catch (const std::exception& e) {
      setErrorCodeAndReason(drogon::k500InternalServerError, e.what(), resp);
      errlog("Exception in command [%s] exception: %s", command, e.what());
    }
  }
  incCommandStat(command);
  incPrometheusCommandMetric(command);
}

void parseCustomGetCmd(const drogon::HttpRequestPtr& req,
                       const std::string& command,
                       const drogon::HttpResponsePtr& resp)
{
  string err;

  try {
    std::string ret_msg = g_luamultip->custom_get_func(command);
    resp->setStatusCode(drogon::k200OK);
    resp->setBody(ret_msg);
  }
  catch (LuaContext::ExecutionErrorException& e) {
    resp->setStatusCode(drogon::k500InternalServerError);
    std::stringstream ss;
    try {
      std::rethrow_if_nested(e);
      setErrorCodeAndReason(drogon::k500InternalServerError, e.what(), resp);
      errlog("Lua custom function [%s] exception: %s", command, e.what());
    }
    catch (const std::exception& ne) {
      setErrorCodeAndReason(drogon::k500InternalServerError, ne.what(), resp);
      errlog("Exception in command [%s] exception: %s", command, ne.what());
    }
    catch (const WforceException& ne) {
      setErrorCodeAndReason(drogon::k500InternalServerError, ne.reason, resp);
      errlog("Exception in command [%s] exception: %s", command, ne.reason);
    }
  }
  catch (const std::exception& e) {
    setErrorCodeAndReason(drogon::k500InternalServerError, e.what(), resp);
    errlog("Exception in command [%s] exception: %s", command, e.what());
  }
  incCommandStat(command + "_Get");
  incPrometheusCommandMetric(command + "_Get");
}

std::atomic<bool> g_ping_up{false};

void parsePingCmd(const drogon::HttpRequestPtr& req,
                  const std::string& command,
                  const drogon::HttpResponsePtr& resp)
{
  resp->setStatusCode(drogon::k200OK);
  if (g_ping_up)
    resp->setBody(R"({"status":"ok"})");
  else
    resp->setBody(R"({"status":"warmup"})");
  incCommandStat("ping");
  incPrometheusCommandMetric("ping");
}

void parseSyncDoneCmd(const drogon::HttpRequestPtr& req,
                      const std::string& command,
                      const drogon::HttpResponsePtr& resp)
{
  resp->setStatusCode(drogon::k200OK);
  g_ping_up = true;

  resp->setBody(R"({"status":"ok"})");
  incCommandStat("syncDone");
  incPrometheusCommandMetric("syncDone");
}

void registerWebserverCommands()
{
  addCommandStat("addWLEntry");
  addPrometheusCommandMetric("addWLEntry");
  g_webserver.registerFunc("addWLEntry", HTTPVerb::POST, WforceWSFunc(parseAddWLEntryCmd));
  addCommandStat("delWLEntry");
  addPrometheusCommandMetric("delWLEntry");
  g_webserver.registerFunc("delWLEntry", HTTPVerb::POST, WforceWSFunc(parseDelWLEntryCmd));
  addCommandStat("addBLEntry");
  addPrometheusCommandMetric("addBLEntry");
  g_webserver.registerFunc("addBLEntry", HTTPVerb::POST, WforceWSFunc(parseAddBLEntryCmd));
  addCommandStat("delBLEntry");
  addPrometheusCommandMetric("delWLEntry");
  g_webserver.registerFunc("delBLEntry", HTTPVerb::POST, WforceWSFunc(parseDelBLEntryCmd));
  addCommandStat("reset");
  addPrometheusCommandMetric("reset");
  g_webserver.registerFunc("reset", HTTPVerb::POST, WforceWSFunc(parseResetCmd));
  addCommandStat("report");
  addPrometheusCommandMetric("report");
  g_webserver.registerFunc("report", HTTPVerb::POST, WforceWSFunc(parseReportCmd));
  addCommandStat("allow");
  addPrometheusCommandMetric("allow");
  addCommandStat("allow_allowed");
  addPrometheusAllowStatusMetric("allowed");
  addCommandStat("allow_blacklisted");
  addPrometheusAllowStatusMetric("blacklisted");
  addCommandStat("allow_whitelisted");
  addPrometheusAllowStatusMetric("whitelisted");
  addCommandStat("allow_denied");
  addPrometheusAllowStatusMetric("denied");
  addCommandStat("allow_tarpitted");
  addPrometheusAllowStatusMetric("tarpitted");
  g_webserver.registerFunc("allow", HTTPVerb::POST, WforceWSFunc(parseAllowCmd));
  addCommandStat("stats");
  addPrometheusCommandMetric("stats");
  g_webserver.registerFunc("stats", HTTPVerb::GET, WforceWSFunc(parseStatsCmd));
  addCommandStat("getBL");
  addPrometheusCommandMetric("getBL");
  g_webserver.registerFunc("getBL", HTTPVerb::GET, WforceWSFunc(parseGetBLCmd));
  addCommandStat("getWL");
  addPrometheusCommandMetric("getWL");
  g_webserver.registerFunc("getWL", HTTPVerb::GET, WforceWSFunc(parseGetWLCmd));
  addCommandStat("getDBStats");
  addPrometheusCommandMetric("getDBStats");
  g_webserver.registerFunc("getDBStats", HTTPVerb::POST, WforceWSFunc(parseGetStatsCmd));
  addCommandStat("ping");
  addPrometheusCommandMetric("ping");
  g_webserver.registerFunc("ping", HTTPVerb::GET, WforceWSFunc(parsePingCmd));
  addCommandStat("syncDBs");
  addPrometheusCommandMetric("syncDBs");
  g_webserver.registerFunc("syncDBs", HTTPVerb::POST, WforceWSFunc(parseSyncCmd));
  addCommandStat("syncDone");
  addPrometheusCommandMetric("syncDone");
  g_webserver.registerFunc("syncDone", HTTPVerb::GET, WforceWSFunc(parseSyncDoneCmd));
  addCommandStat("dumpEntries");
  addPrometheusCommandMetric("dumpEntries");
  g_webserver.registerFunc("dumpEntries", HTTPVerb::POST, WforceWSFunc(parseDumpEntriesCmd));
  addCommandStat("addSibling");
  addPrometheusCommandMetric("addSibling");
  g_webserver.registerFunc("addSibling", HTTPVerb::POST, WforceWSFunc(parseAddSiblingCmd));
  addCommandStat("removeSibling");
  addPrometheusCommandMetric("removeSibling");
  g_webserver.registerFunc("removeSibling", HTTPVerb::POST, WforceWSFunc(parseRemoveSiblingCmd));
  addCommandStat("setSiblings");
  addPrometheusCommandMetric("setSiblings");
  g_webserver.registerFunc("setSiblings", HTTPVerb::POST, WforceWSFunc(parseSetSiblingsCmd));
}
