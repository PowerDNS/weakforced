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
#include "yahttp/yahttp.hpp"
#include "namespaces.hh"
#include <sys/time.h>
#include <sys/resource.h>
#include "ext/ctpl.h"
#include "base64.hh"
#include "blacklist.hh"
#include "twmap-wrapper.hh"
#include "customfunc.hh"
#include "perf-stats.hh"
#include "luastate.hh"
#include "login_tuple.hh"
#include "webhook.hh"
#include "boost/date_time/posix_time/posix_time.hpp"
#include "boost/date_time/gregorian/gregorian.hpp"

using std::thread;
using namespace boost::posix_time;
using namespace boost::gregorian;

GlobalStateHolder<vector<shared_ptr<Sibling>>> g_report_sinks;
GlobalStateHolder<std::map<std::string, std::pair<std::shared_ptr<std::atomic<unsigned int>>, vector<shared_ptr<Sibling>>>>> g_named_report_sinks;

static int uptimeOfProcess()
{
  static time_t start=time(0);
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

bool g_allowlog_verbose = false;

void allowLog(int retval, const std::string& msg, const LoginTuple& lt, const std::vector<pair<std::string, std::string>>& kvs) 
{
  std::ostringstream os;
  if ((retval != 0) || g_allowlog_verbose) {
    os << "allowLog " << msg << ": ";
    os << "allow=\"" << retval << "\" ";
    os << "remote=\"" << lt.remote.toString() << "\" ";
    os << "login=\"" << lt.login << "\" ";
    os << "protocol=\"" << lt.protocol << "\" ";
    os << "device_id=\"" << lt.device_id << "\" ";
    os << DeviceAttrsToString(lt);
    os << LtAttrsToString(lt);
    os << "rattrs={";
    for (const auto& i : kvs) {
      os << i.first << "="<< "\"" << i.second << "\"" << " ";
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

void addBLEntries(const std::vector<BlackListEntry>& blv, const char* key_name, json11::Json::array& my_entries)
{
  using namespace json11;
  for (auto i = blv.begin(); i != blv.end(); ++i) {
    Json my_entry = Json::object {
      { key_name, (std::string)i->key },   
      { "expiration", (std::string)boost::posix_time::to_simple_string(i->expiration)},
      { "reason", (std::string)i->reason }
    };
    my_entries.push_back(my_entry);
  }
}

bool canonicalizeLogin(std::string& login, YaHTTP::Response& resp)
{
  bool retval = true;
  
  try {
    // canonicalize the login - e.g. turn "foo" into "foo@foobar.com" and bar into "bar@barfoo.com"
    login = g_luamultip->canonicalize(login);
  }
  catch(LuaContext::ExecutionErrorException& e) {
    resp.status=500;
    std::stringstream ss;
    ss << "{\"status\":\"failure\", \"reason\":\"" << e.what() << "\"}";
    resp.body=ss.str();
    errlog("Lua canonicalize function exception: %s", e.what());
    retval = false;
  }
  catch(...) {
    resp.status=500;
    resp.body=R"({"status":"failure"})";
    retval = false;
  }
  return retval;
}

void parseSyncCmd(const YaHTTP::Request& req, YaHTTP::Response& resp, const std::string& command)
{
  using namespace json11;
  Json msg;
  string err;

  resp.status = 200;

  msg = Json::parse(req.body, err);
  if (msg.is_null()) {
    resp.status=500;
    std::stringstream ss;
    ss << "{\"status\":\"failure\", \"reason\":\"" << err << "\"}";
    resp.body=ss.str();
  }
  else {
    try {
      if (msg["replication_host"].is_null() ||
          msg["replication_port"].is_null() ||
          msg["callback_url"].is_null() ||
          msg["callback_auth_pw"].is_null()) {
        throw WforceException("One of mandatory parameters [replication_host, replication_port, callback_url, callback_auth_pw] is missing");
      }
      string myip = msg["replication_host"].string_value();
      int myport = msg["replication_port"].int_value();
      ComboAddress replication_ca(myip, myport);
      std::string callback_url = msg["callback_url"].string_value();
      std::string callback_pw = msg["callback_auth_pw"].string_value();
      thread t(syncDBThread, replication_ca, callback_url, callback_pw);
      t.detach();
    }
    catch(const WforceException& e) {
      resp.status=500;
      std::stringstream ss;
      ss << "{\"status\":\"failure\", \"reason\":\"" << e.reason << "\"}";
      resp.body=ss.str();
      errlog("Exception in command [%s] exception: %s", command, e.reason);
    }
  }
  if (resp.status == 200)
    resp.body=R"({"status":"ok"})";
  incCommandStat("SyncDBs");
}

void parseAddDelBLEntryCmd(const YaHTTP::Request& req, YaHTTP::Response& resp, bool addCmd)
{
  using namespace json11;
  Json msg;
  string err;

  resp.status = 200;

  msg=Json::parse(req.body, err);
  if (msg.is_null()) {
    resp.status=500;
    std::stringstream ss;
    ss << "{\"status\":\"failure\", \"reason\":\"" << err << "\"}";
    resp.body=ss.str();
  }
  else {
    bool haveIP=false;
    bool haveLogin=false;
    bool haveNetmask=false;
    unsigned int bl_seconds=0;
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
	if (addCmd)
	  g_bl_db.addEntry(en_ca, en_login, bl_seconds, bl_reason);
	else
	  g_bl_db.deleteEntry(en_ca, en_login);
      }
      else if (haveLogin) {
	if (addCmd)
	  g_bl_db.addEntry(en_login, bl_seconds, bl_reason);
	else
	  g_bl_db.deleteEntry(en_login);
      }
      else if (haveIP || haveNetmask) {
	if (addCmd)
	  g_bl_db.addEntry(en_nm, bl_seconds, bl_reason);
	else
	  g_bl_db.deleteEntry(en_nm);
      }
    }
    catch (std::runtime_error& e) {
      resp.status=500;
      std::stringstream ss;
      ss << "{\"status\":\"failure\", \"reason\":\"" << e.what() << "\"}";
      resp.body=ss.str();    }
    catch(...) {
      resp.status=500;
      resp.body=R"({"status":"failure"})";
    }
  }
  if (resp.status == 200)
    resp.body=R"({"status":"ok"})";
}

void parseAddBLEntryCmd(const YaHTTP::Request& req, YaHTTP::Response& resp, const std::string& command)
{
  parseAddDelBLEntryCmd(req, resp, true);
  incCommandStat("addBLEntry");
}

void parseDelBLEntryCmd(const YaHTTP::Request& req, YaHTTP::Response& resp, const std::string& command)
{
  parseAddDelBLEntryCmd(req, resp, false);
  incCommandStat("delBLEntry");
}

void parseResetCmd(const YaHTTP::Request& req, YaHTTP::Response& resp, const std::string& command)
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
      bool haveIP=false;
      bool haveLogin=false;
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
	resp.status = 415;
	resp.body=R"({"status":"failure", "reason":"No ip or login field supplied"})";
      }
      else {
	bool reset_ret;
	{
	  reset_ret = g_luamultip->reset(en_type, en_login, en_ca);
	}
	resp.status = 200;
	if (reset_ret)
	  resp.body=R"({"status":"ok"})";
	else
	  resp.body=R"({"status":"failure", "reason":"reset function returned false"})";
      }
      // generate webhook events
      Json jobj = Json::object{{"login", en_login}, {"ip", en_ca.toString()}};
      std::string hook_data = jobj.dump();
      for (const auto& h : g_webhook_db.getWebHooksForEvent("reset")) {
	if (auto hs = h.lock())
	  g_webhook_runner.runHook("reset", hs, hook_data);
      }
    }
    catch(LuaContext::ExecutionErrorException& e) {
      resp.status=500;
      std::stringstream ss;
      ss << "{\"status\":\"failure\", \"reason\":\"" << e.what() << "\"}";
      resp.body=ss.str();
      errlog("Lua reset function exception: %s", e.what());
    }
    catch(...) {
      resp.status=500;
      resp.body=R"({"status":"failure"})";
    }
  }
  incCommandStat("reset");
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

      lt.from_json(msg, g_ua_parser_p);

      // canonicalize the login - e.g. turn "foo" into "foo@foobar.com" and bar into "bar@barfoo.com"
      if (!canonicalizeLogin(lt.login, resp))
	return;
      lt.t=getDoubleTime(); // set the time
      reportLog(lt);
      g_stats.reports++;
      resp.status=200;
      {
	g_luamultip->report(lt);
      }

      // If any normal or named report sinks are configured, send the report to one of them
      sendReportSink(lt); // XXX - this is deprecated now in favour of NamedReportSinks
      sendNamedReportSink(lt.serialize());
      
      std::string hook_data = lt.serialize();
      for (const auto& h : g_webhook_db.getWebHooksForEvent("report")) {
	if (auto hs = h.lock())
	  g_webhook_runner.runHook("report", hs, hook_data);
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
  incCommandStat("report");
}

#define OX_PROTECT_NOTIFY 0

std::string genProtectHookData(const Json& lt, const std::string& msg)
{
  Json::object jobj;

  double dtime = lt["t"].number_value();
  unsigned int isecs = (int)dtime;
  unsigned int iusecs = (dtime - isecs) * 1000000;
  
  ptime t(date(1970,Jan,01), seconds(isecs)+microsec(iusecs));
  jobj.insert(std::make_pair("timestamp", to_iso_extended_string(t)+"Z"));

  jobj.insert(std::make_pair("user", lt["login"].string_value()));
  jobj.insert(std::make_pair("device", lt["device_id"].string_value()));
  jobj.insert(std::make_pair("ip", lt["remote"].string_value()));
  jobj.insert(std::make_pair("message", msg));
  jobj.insert(std::make_pair("code", OX_PROTECT_NOTIFY));
  jobj.insert(std::make_pair("application", "wforce"));

  return Json(jobj).dump();
}

bool allow_filter(std::shared_ptr<const WebHook> hook, int status)
{
  bool retval = false;
  if (hook->hasConfigKey("allow_filter")) {
    std::string filter = hook->getConfigKey("allow_filter");
    if (((filter.find("reject")!=string::npos) && (status < 0)) ||
	(((filter.find("allow")!=string::npos) && (status == 0))) ||
	(((filter.find("tarpit")!=string::npos) && (status > 0)))) {
      retval = true;
      vdebuglog("allow_filter: filter evaluates to true (allowing event)");
    }
  }
  else
    retval = true;
  
  return retval;
}

enum AllowReturnFields { allowRetStatus=0, allowRetMsg=1, allowRetLogMsg=2, allowRetAttrs=3 };

void parseAllowCmd(const YaHTTP::Request& req, YaHTTP::Response& resp, const std::string& command)
{
  using namespace json11;
  Json msg;
  string err;
  std::vector<pair<std::string, std::string>> ret_attrs;

  msg=Json::parse(req.body, err);
  if (msg.is_null()) {
    resp.status=500;
    std::stringstream ss;
    ss << "{\"status\":\"failure\", \"reason\":\"" << err << "\"}";
    resp.body=ss.str();
  }
  else {
    LoginTuple lt;
    int status = -1;
    std::string ret_msg;

    try {
      lt.from_json(msg, g_ua_parser_p);
      lt.t=getDoubleTime();
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

    if (!canonicalizeLogin(lt.login, resp))
      return;
    
    // first check the built-in blacklists
    BlackListEntry ble;
    if (g_bl_db.getEntry(lt.remote, ble)) {
      std::vector<pair<std::string, std::string>> log_attrs = 
	{ { "expiration", boost::posix_time::to_simple_string(ble.expiration) } };
      allowLog(status, std::string("blacklisted IP"), lt, log_attrs);
      ret_msg = "Temporarily blacklisted IP Address - try again later";
      incCommandStat("allow_blacklisted");
    }
    else if (g_bl_db.getEntry(lt.login, ble)) {
      std::vector<pair<std::string, std::string>> log_attrs = 
	{ { "expiration", boost::posix_time::to_simple_string(ble.expiration) } };
      allowLog(status, std::string("blacklisted Login"), lt, log_attrs);	  
      ret_msg = "Temporarily blacklisted Login Name - try again later";
      incCommandStat("allow_blacklisted");
    }
    else if (g_bl_db.getEntry(lt.remote, lt.login, ble)) {
      std::vector<pair<std::string, std::string>> log_attrs = 
	{ { "expiration", boost::posix_time::to_simple_string(ble.expiration) } };
      allowLog(status, std::string("blacklisted IPLogin"), lt, log_attrs);	  	  
      ret_msg = "Temporarily blacklisted IP/Login Tuple - try again later";
      incCommandStat("allow_blacklisted");
    }
    else {
      try {
	AllowReturn ar;
	{
	  ar=g_luamultip->allow(lt);
	}
	status = std::get<allowRetStatus>(ar);
	ret_msg = std::get<allowRetMsg>(ar);
	std::string log_msg = std::get<allowRetLogMsg>(ar);
	std::vector<pair<std::string, std::string>> log_attrs = std::get<allowRetAttrs>(ar);

	// log the results of the allow function
	allowLog(status, log_msg, lt, log_attrs);

	ret_attrs = std::move(log_attrs);
	g_stats.allows++;
	if(status < 0) {
	  g_stats.denieds++;
          incCommandStat("allow_denied");
        }
        else if (status > 0) {
          incCommandStat("allow_tarpitted");
        } else {
          incCommandStat("allow_allowed");
        }
	Json::object jattrs;
	for (auto& i : ret_attrs) {
	  jattrs.insert(make_pair(i.first, Json(i.second)));
	}
	msg=Json::object{{"status", status}, {"msg", ret_msg}, {"r_attrs", jattrs}};

	// generate webhook events
	Json jobj = Json::object{{"request", lt.to_json()}, {"response", msg}};
	std::string hook_data = jobj.dump();
	for (const auto& h : g_webhook_db.getWebHooksForEvent("allow")) {
	  if (auto hs = h.lock()) {
	    if (hs->hasConfigKey("ox-protect")) {
	      hook_data = genProtectHookData(jobj["request"],
					     hs->getConfigKey("ox-protect"));
	    }
	    if (allow_filter(hs, status))
	      g_webhook_runner.runHook("allow", hs, hook_data);
	  }
	}
	resp.status=200;
	resp.body=msg.dump();
	return;
      }
      catch(LuaContext::ExecutionErrorException& e) {
	resp.status=500;
	std::stringstream ss;
	try {
	  std::rethrow_if_nested(e);
	  ss << "{\"status\":\"failure\", \"reason\":\"" << e.what() << "\"}";
	  resp.body=ss.str();
	  errlog("Lua function [%s] exception: %s", command, e.what());
	  return;
	}
	catch (const std::exception& ne) {
	  resp.status=500;
	  std::stringstream ss;
	  ss << "{\"status\":\"failure\", \"reason\":\"" << ne.what() << "\"}";
	  resp.body=ss.str();
	  errlog("Exception in command [%s] exception: %s", command, ne.what());
	  return;
	}
	catch (const WforceException& ne) {
	  resp.status=500;
	  std::stringstream ss;
	  ss << "{\"status\":\"failure\", \"reason\":\"" << ne.reason << "\"}";
	  resp.body=ss.str();
	  errlog("Exception in command [%s] exception: %s", command, ne.reason);
	  return;
	}
      }
      catch(const std::exception& e) {
	resp.status=500;
	std::stringstream ss;
	ss << "{\"status\":\"failure\", \"reason\":\"" << e.what() << "\"}";
	resp.body=ss.str();
	errlog("Exception in command [%s] exception: %s", command, e.what());
	return;
      }
      catch(const WforceException& e) {
	resp.status=500;
	std::stringstream ss;
	ss << "{\"status\":\"failure\", \"reason\":\"" << e.reason << "\"}";
	resp.body=ss.str();
	errlog("Exception in command [%s] exception: %s", command, e.reason);
      }
    }
    msg=Json::object{{"status", status}, {"msg", ret_msg}, {"r_attrs", Json::object{}}};
    resp.status=200;
    resp.body=msg.dump();
  }
  incCommandStat("allow");
}

void parseStatsCmd(const YaHTTP::Request& req, YaHTTP::Response& resp, const std::string& command)
{
  using namespace json11;
  struct rusage ru;
  getrusage(RUSAGE_SELF, &ru);

  resp.status=200;
  Json my_json = Json::object {
    { "reports", (int)g_stats.reports },
    { "allows", (int)g_stats.allows },
    { "denieds", (int)g_stats.denieds },
    { "user-msec", (int)(ru.ru_utime.tv_sec*1000ULL + ru.ru_utime.tv_usec/1000) },
    { "sys-msec", (int)(ru.ru_stime.tv_sec*1000ULL + ru.ru_stime.tv_usec/1000) },
    { "uptime", uptimeOfProcess()},
    { "perfstats", perfStatsToJson()},
    { "commandstats", commandStatsToJson()},
    { "customstats", customStatsToJson()}
  };

  resp.status=200;
  resp.body=my_json.dump();
  incCommandStat("stats");
}

void parseGetBLCmd(const YaHTTP::Request& req, YaHTTP::Response& resp, const std::string& command)
{
  using namespace json11;
  Json::array my_entries;

  std::vector<BlackListEntry> blv = g_bl_db.getIPEntries();
  addBLEntries(blv, "ip", my_entries);
  blv = g_bl_db.getLoginEntries();
  addBLEntries(blv, "login", my_entries);
  blv = g_bl_db.getIPLoginEntries();
  addBLEntries(blv, "iplogin", my_entries);
      
  Json ret_json = Json::object {
    { "bl_entries", my_entries }
  };
  resp.status=200;
  resp.body = ret_json.dump();
  incCommandStat("getBL");
}

void parseGetStatsCmd(const YaHTTP::Request& req, YaHTTP::Response& resp, const std::string& command)
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
    bool haveIP=false;
    bool haveLogin=false;
    std::string en_type, en_login;
    std::string key_name, key_value;
    TWKeyType lookup_key;
    bool is_blacklisted;
    ComboAddress en_ca;

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
	is_blacklisted = g_bl_db.checkEntry(en_ca, en_login);
      }
      else if (haveLogin) {
	key_name = "login";
	key_value = en_login;
	lookup_key = en_login;
	is_blacklisted = g_bl_db.checkEntry(en_login);
      }
      else if (haveIP) {
	key_name = "ip";
	key_value = en_ca.toString();
	lookup_key = en_ca;
	is_blacklisted = g_bl_db.checkEntry(en_ca);
      }
    }
    catch(...) {
	resp.status=500;
	resp.body=R"({"status":"failure", "reason":"Could not parse input"})";
	return;
    }
    
    if (!haveLogin && !haveIP) {
      resp.status = 415;
      resp.body=R"({"status":"failure", "reason":"No ip or login field supplied"})";
    } 
    else {
      Json::object js_db_stats;
      {
	std::lock_guard<std::mutex> lock(dbMap_mutx);
	for (auto i = dbMap.begin(); i != dbMap.end(); ++i) {
	  std::string dbName = i->first;
	  std::vector<std::pair<std::string, int>> db_fields;
	  if (i->second.get_all_fields(lookup_key, db_fields)) {
	    Json::object js_db_fields;
	    for (auto j = db_fields.begin(); j != db_fields.end(); ++j) {
	      js_db_fields.insert(make_pair(j->first, j->second));
	    }
	    js_db_stats.insert(make_pair(dbName, js_db_fields));
	  }
	}
      }
      Json ret_json = Json::object {
	{ key_name, key_value },
	{ "blacklisted", is_blacklisted },
	{ "stats", js_db_stats }
      };
      resp.status=200;
      resp.body = ret_json.dump();  
    }
  }
  incCommandStat("getDBStats");
}

enum CustomReturnFields { customRetStatus=0, customRetAttrs=1 };

void parseCustomCmd(const YaHTTP::Request& req, YaHTTP::Response& resp, const std::string& command)
{
  using namespace json11;
  Json msg;
  string err;
  KeyValVector ret_attrs;

  msg=Json::parse(req.body, err);
  if (msg.is_null()) {
    resp.status=500;
    std::stringstream ss;
    ss << "{\"success\":\"failure\", \"reason\":\"" << err << "\"}";
    resp.body=ss.str();
  }
  else {
    CustomFuncArgs cfa;
    bool status = false;
    
    try {
      cfa.setAttrs(msg);
    }
    catch(...) {
      resp.status=500;
      resp.body=R"({"success":false, "reason":"Could not parse input"})";
      return;
    }

    try {
      bool reportSink=false;
      CustomFuncReturn cr;
      {
	cr=g_luamultip->custom_func(command, cfa, reportSink);
      }
      status = std::get<customRetStatus>(cr);
      ret_attrs = std::get<customRetAttrs>(cr);
      Json::object jattrs;
      for (auto& i : ret_attrs) {
	jattrs.insert(make_pair(i.first, Json(i.second)));
      }
      msg=Json::object{{"success", status}, {"r_attrs", jattrs}};

      // send the custom parameters to the report sink if configured
      if (reportSink)
	sendNamedReportSink(cfa.serialize());
      
      resp.status=200;
      resp.body=msg.dump();
    }
    catch(LuaContext::ExecutionErrorException& e) {
      resp.status=500;
      std::stringstream ss;
      try {
	std::rethrow_if_nested(e);
	ss << "{\"success\":false, \"reason\":\"" << e.what() << "\"}";
	resp.body=ss.str();
	errlog("Lua custom function [%s] exception: %s", command, e.what());
      }
      catch (const std::exception& ne) {
	resp.status=500;
	std::stringstream ss;
	ss << "{\"success\":false, \"reason\":\"" << ne.what() << "\"}";
	resp.body=ss.str();
	errlog("Exception in command [%s] exception: %s", command, ne.what());
      }
      catch (const WforceException& ne) {
	resp.status=500;
	std::stringstream ss;
	ss << "{\"success\":false, \"reason\":\"" << ne.reason << "\"}";
	resp.body=ss.str();
	errlog("Exception in command [%s] exception: %s", command, ne.reason);
      }
    }
    catch(const std::exception& e) {
      resp.status=500;
      std::stringstream ss;
      ss << "{\"success\":false, \"reason\":\"" << e.what() << "\"}";
      resp.body=ss.str();
      errlog("Exception in command [%s] exception: %s", command, e.what());
    }
  }
  incCommandStat(command);
}

bool g_ping_up = false;

void parsePingCmd(const YaHTTP::Request& req, YaHTTP::Response& resp, const std::string& command)
{
  resp.status = 200;
  if (g_ping_up)
    resp.body = R"({"status":"ok"})";
  else
    resp.body = R"({"status":"warmup"})";
  incCommandStat("ping");
}

void parseSyncDoneCmd(const YaHTTP::Request& req, YaHTTP::Response& resp, const std::string& command)
{
  resp.status = 200;
  g_ping_up = true;
  
  resp.body = R"({"status":"ok"})";
  incCommandStat("syncDone");
}

void registerWebserverCommands()
{
  addCommandStat("addBLEntry");
  g_webserver.registerFunc("addBLEntry", HTTPVerb::POST, parseAddBLEntryCmd);
  addCommandStat("delBLEntry");
  g_webserver.registerFunc("delBLEntry", HTTPVerb::POST, parseDelBLEntryCmd);
  addCommandStat("reset");
  g_webserver.registerFunc("reset", HTTPVerb::POST, parseResetCmd);
  addCommandStat("report");
  g_webserver.registerFunc("report", HTTPVerb::POST, parseReportCmd);
  addCommandStat("allow");
  addCommandStat("allow_allowed");
  addCommandStat("allow_blacklisted");
  addCommandStat("allow_denied");
  addCommandStat("allow_tarpitted");
  g_webserver.registerFunc("allow", HTTPVerb::POST, parseAllowCmd);
  addCommandStat("stats");
  g_webserver.registerFunc("stats", HTTPVerb::GET, parseStatsCmd);
  addCommandStat("getBL");
  g_webserver.registerFunc("getBL", HTTPVerb::GET, parseGetBLCmd);
  addCommandStat("getDBStats");
  g_webserver.registerFunc("getDBStats", HTTPVerb::POST, parseGetStatsCmd);
  addCommandStat("ping");
  g_webserver.registerFunc("ping", HTTPVerb::GET, parsePingCmd);
  addCommandStat("syncDBs");
  g_webserver.registerFunc("syncDBs", HTTPVerb::POST, parseSyncCmd);
  addCommandStat("syncDone");
  g_webserver.registerFunc("syncDone", HTTPVerb::GET, parseSyncDoneCmd);
}
