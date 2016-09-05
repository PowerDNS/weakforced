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

#include "wforce.hh"
#include "sstuff.hh"
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
#include "blacklist.hh"
#include "twmap-wrapper.hh"
#include "perf-stats.hh"
#include "webhook.hh"

using std::thread;

static int uptimeOfProcess()
{
  static time_t start=time(0);
  return time(0) - start;
}

bool compareAuthorization(YaHTTP::Request& req, const string &expected_password)
{
  // validate password
  YaHTTP::strstr_map_t::iterator header = req.headers.find("authorization");
  bool auth_ok = false;
  if (header != req.headers.end() && toLower(header->second).find("basic ") == 0) {
    string cookie = header->second.substr(6);

    string plain;
    B64Decode(cookie, plain);

    vector<string> cparts;
    stringtok(cparts, plain, ":");

    // this gets rid of terminating zeros
    auth_ok = (cparts.size()==2 && (0==strcmp(cparts[1].c_str(), expected_password.c_str())));
  }
  return auth_ok;
}

std::string LtAttrsToString(const LoginTuple& lt)
{
  std::ostringstream os;
  os << "attrs={";
  for (auto i= lt.attrs.begin(); i!=lt.attrs.end(); ++i) {
    os << i->first << "="<< "\"" << i->second << "\"";
    if (i != --(lt.attrs.end()))
      os << ", ";
  }
  for (auto i = lt.attrs_mv.begin(); i!=lt.attrs_mv.end(); ++i) {
    if (i == lt.attrs_mv.begin())
      os << ", ";
    os << i->first << "=[";
    std::vector<std::string> vec = i->second;
    for (auto j = vec.begin(); j!=vec.end(); ++j) {
      os << "\"" << *j << "\"";
      if (j != --(vec.end()))
	os << ", ";
    }
    os << "]";
    if (i != --(lt.attrs_mv.end()))
      os << ", ";
  }
  os << "} ";
  return os.str();
}

void reportLog(const LoginTuple& lt)
{
  std::ostringstream os;
  os << "reportLog: ";
  os << "remote=\"" << lt.remote.toString() << "\" ";
  os << "login=\"" << lt.login << "\" ";
  os << "success=\"" << lt.success << "\" ";
  os << "policy_reject=\"" << lt.policy_reject << "\" ";
  os << "pwhash=\"" << std::hex << std::uppercase << lt.pwhash << "\" ";
  os << LtAttrsToString(lt);
  infolog(os.str().c_str());
}

void allowLog(int retval, const std::string& msg, const LoginTuple& lt, const std::vector<pair<std::string, std::string>>& kvs) 
{
  std::ostringstream os;
  os << "allowLog " << msg << ": ";
  os << "allow=\"" << retval << "\" ";
  os << "remote=\"" << lt.remote.toString() << "\" ";
  os << "login=\"" << lt.login << "\" ";
  os << LtAttrsToString(lt);
  for (const auto& i : kvs) {
    os << i.first << "="<< "\"" << i.second << "\"" << " ";
  }
  // only log at notice if login was rejected or tarpitted
  if (retval == 0)
    infolog(os.str().c_str());
  else
    noticelog(os.str().c_str());
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
    unsigned int bl_seconds=0;
    std::string bl_reason;
    std::string en_login;
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
      else if (haveIP) {
	if (addCmd)
	  g_bl_db.addEntry(en_ca, bl_seconds, bl_reason);
	else
	  g_bl_db.deleteEntry(en_ca);
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

void parseResetCmd(const YaHTTP::Request& req, YaHTTP::Response& resp)
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
	g_webhook_runner.runHook("reset", h, hook_data);
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
}

void parseReportCmd(const YaHTTP::Request& req, YaHTTP::Response& resp)
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

      lt.from_json(msg);

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

      std::string hook_data = lt.serialize();
      for (const auto& h : g_webhook_db.getWebHooksForEvent("report")) {	
	g_webhook_runner.runHook("report", h, hook_data);
      }

      resp.status=200;
      resp.body=R"({"status":"ok"})";
    }
    catch(LuaContext::ExecutionErrorException& e) {
      resp.status=500;
      std::stringstream ss;
      ss << "{\"status\":\"failure\", \"reason\":\"" << e.what() << "\"}";
      resp.body=ss.str();
      errlog("Lua report function exception: %s", e.what());
    }
    catch(...) {
      resp.status=500;
      resp.body=R"({"status":"failure"})";
    }
  }
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
      debuglog("allow_filter: filter evaluates to true (allowing event)");
    }
  }
  else
    retval = true;
  
  return retval;
}

void parseAllowCmd(const YaHTTP::Request& req, YaHTTP::Response& resp)
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
    LoginTuple lt;
    int status = -1;
    std::string ret_msg;

    try {
      lt.from_json(msg);
      lt.t=getDoubleTime();
    }
    catch(...) {
	resp.status=500;
	resp.body=R"({"status":"failure", "reason":"Could not parse input"})";
	return;
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
    }
    else if (g_bl_db.getEntry(lt.login, ble)) {
      std::vector<pair<std::string, std::string>> log_attrs = 
	{ { "expiration", boost::posix_time::to_simple_string(ble.expiration) } };
      allowLog(status, std::string("blacklisted Login"), lt, log_attrs);	  
      ret_msg = "Temporarily blacklisted Login Name - try again later";
    }
    else if (g_bl_db.getEntry(lt.remote, lt.login, ble)) {
      std::vector<pair<std::string, std::string>> log_attrs = 
	{ { "expiration", boost::posix_time::to_simple_string(ble.expiration) } };
      allowLog(status, std::string("blacklisted IPLogin"), lt, log_attrs);	  	  
      ret_msg = "Temporarily blacklisted IP/Login Tuple - try again later";
    }
    else {
      try {
	AllowReturn ar;
	{
	  ar=g_luamultip->allow(lt);
	}
	status = std::get<0>(ar);
	ret_msg = std::get<1>(ar);
	std::string log_msg = std::get<2>(ar);
	std::vector<pair<std::string, std::string>> log_attrs = std::get<3>(ar);

	// log the results of the allow function
	allowLog(status, log_msg, lt, log_attrs);
      }
      catch(LuaContext::ExecutionErrorException& e) {
	resp.status=500;
	std::stringstream ss;
	ss << "{\"status\":\"failure\", \"reason\":\"" << e.what() << "\"}";
	resp.body=ss.str();
	errlog("Lua allow function exception: %s", e.what());
      }
      catch(...) {
	resp.status=500;
	resp.body=R"({"status":"failure"})";
      }
    }
    g_stats.allows++;
    if(status < 0)
      g_stats.denieds++;
    msg=Json::object{{"status", status}, {"msg", ret_msg}};

    // generate webhook events
    Json jobj = Json::object{{"request", lt.to_json()}, {"response", msg}};
    std::string hook_data = jobj.dump();
    for (const auto& h : g_webhook_db.getWebHooksForEvent("allow")) {	
      if (allow_filter(h, status))
	g_webhook_runner.runHook("allow", h, hook_data);
    }
    
    resp.status=200;
    resp.body=msg.dump();
  }  
}

void parseStatsCmd(const YaHTTP::Request& req, YaHTTP::Response& resp)
{
  using namespace json11;
  struct rusage ru;
  getrusage(RUSAGE_SELF, &ru);

  resp.status=200;
  Json my_json = Json::object {
    { "allows", (int)g_stats.allows },
    { "denieds", (int)g_stats.denieds },
    { "user-msec", (int)(ru.ru_utime.tv_sec*1000ULL + ru.ru_utime.tv_usec/1000) },
    { "sys-msec", (int)(ru.ru_stime.tv_sec*1000ULL + ru.ru_stime.tv_usec/1000) },
    { "uptime", uptimeOfProcess()},
    { "qa-latency", (int)g_stats.latency}
  };

  resp.status=200;
  resp.body=my_json.dump();
}

void parseGetBLCmd(const YaHTTP::Request& req, YaHTTP::Response& resp)
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
}

void parseGetStatsCmd(const YaHTTP::Request& req, YaHTTP::Response& resp)
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
}

struct WFConnection {
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
static WFCArray sock_vec;
static std::mutex sock_vec_mutx;

static void connectionThread(int id, std::shared_ptr<WFConnection> wfc)
{
  using namespace json11;
  string line;
  string request;
  YaHTTP::Request req;
  bool keepalive = false;
  bool closeConnection=true;
  bool validRequest = true;

  if (!wfc) 
    return;

  auto start_time = std::chrono::steady_clock::now();
  auto wait_time = start_time - wfc->q_entry_time;
  auto i_millis = std::chrono::duration_cast<std::chrono::milliseconds>(wait_time);
  addWTWStat(i_millis.count());

  infolog("Webserver handling request from %s on fd=%d", wfc->remote.toStringWithPort(), wfc->fd);

  YaHTTP::AsyncRequestLoader yarl;
  yarl.initialize(&req);
  int timeout = 5; // XXX make this configurable
  wfc->s.setNonBlocking();
  bool complete=false;
  try {
    while(!complete) {
      int bytes;
      char buf[1024];
      bytes = wfc->s.readWithTimeout(buf, sizeof(buf), timeout);
      if (bytes > 0) {
	string data(buf, bytes);
	complete = yarl.feed(data);
      } else {
	// read error OR EOF
	validRequest = false;
	break;
      }
    }
    yarl.finalize();
  } catch (YaHTTP::ParseError &e) {
    // request stays incomplete
    infolog("Unparseable HTTP request from %s", wfc->remote.toStringWithPort());
    validRequest = false;
  } catch (NetworkError& e) {
    warnlog("Network error in web server: %s", e.what());
    validRequest = false;
  }

  if (validRequest) {
    string conn_header = req.headers["Connection"];
    if (conn_header.compare("keep-alive") == 0)
      keepalive = true;

    if (conn_header.compare("close") == 0)
      closeConnection = true;
    else if (req.version > 10 || ((req.version < 11) && (keepalive == true)))
      closeConnection = false;

    string command=req.getvars["command"];

    string callback;

    if(req.getvars.count("callback")) {
      callback=req.getvars["callback"];
      req.getvars.erase("callback");
    }

    req.getvars.erase("_"); // jQuery cache buster

    YaHTTP::Response resp;
    resp.headers["Content-Type"] = "application/json";
    if (closeConnection)
      resp.headers["Connection"] = "close";
    else if (keepalive == true)
      resp.headers["Connection"] = "keep-alive";
    resp.version = req.version;
    resp.url = req.url;

    string ctype = req.headers["Content-Type"];
    if (!compareAuthorization(req, wfc->password)) {
      errlog("HTTP Request \"%s\" from %s: Web Authentication failed", req.url.path, wfc->remote.toStringWithPort());
      resp.status=401;
      std::stringstream ss;
      ss << "{\"status\":\"failure\", \"reason\":" << "Unauthorized" << "}";
      resp.body=ss.str();
      resp.headers["WWW-Authenticate"] = "basic realm=\"wforce\"";
    }
    else if (command=="ping" && req.method=="GET") {
      resp.status = 200;
      resp.body=R"({"status":"ok"})";
    }
    else if(command=="stats") {
      parseStatsCmd(req, resp);
    }
    else if(command=="getBL") {
      parseGetBLCmd(req, resp);
    }
    else if ((command != "") && (ctype.compare("application/json") != 0)) {
      errlog("HTTP Request \"%s\" from %s: Content-Type not application/json", req.url.path, wfc->remote.toStringWithPort());
      resp.status = 415;
      std::stringstream ss;
      ss << "{\"status\":\"failure\", \"reason\":" << "\"Invalid Content-Type - must be application/json\"" << "}";
      resp.body=ss.str();
    }
    else if (command=="reset" && req.method=="POST") {
      parseResetCmd(req, resp);
    }
    else if(command=="report" && req.method=="POST") {
      parseReportCmd(req, resp);
    }
    else if(command=="allow" && req.method=="POST") {
      parseAllowCmd(req, resp);
    }
    else if (command=="addBLEntry" && req.method=="POST") {
      parseAddDelBLEntryCmd(req, resp, true);
    }
    else if (command=="delBLEntry" && req.method=="POST") {
      parseAddDelBLEntryCmd(req, resp, false);
    }
    else if(command=="getDBStats" && req.method=="POST") {
      parseGetStatsCmd(req, resp);
    }
    else {
      // cerr<<"404 for: "<<resp.url.path<<endl;
      resp.status=404;
    }

    if(!callback.empty()) {
      resp.body = callback + "(" + resp.body + ");";
    }

    std::ostringstream ofs;
    ofs << resp;
    string done;
    done=ofs.str();
    writen2(wfc->fd, done.c_str(), done.size());
  }

  {
    std::lock_guard<std::mutex> lock(sock_vec_mutx);
    if (closeConnection) {
      wfc->closeConnection = true;
    }
    wfc->inConnectionThread = false;
    auto end_time = std::chrono::steady_clock::now();
    auto run_time = end_time - start_time;
    auto i_millis = std::chrono::duration_cast<std::chrono::milliseconds>(run_time);
    addWTRStat(i_millis.count());
    return;
  }
}

unsigned int g_num_worker_threads = WFORCE_NUM_WORKER_THREADS;

#include "poll.h"

void pollThread()
{
  ctpl::thread_pool p(g_num_worker_threads, 5000);

  for (;;) {
    // parse the array of sockets and create a pollfd array
    struct pollfd* fds;
    int num_fds=0;
    {
      std::lock_guard<std::mutex> lock(sock_vec_mutx);
      num_fds = sock_vec.size();
      fds = new struct pollfd [num_fds];
      if (!fds) {
	errlog("Cannot allocate memory in pollThread()");
	exit(-1);
      }
      int j=0;
      // we want to keep the symmetry between pollfds and sock_vec in terms of number and order of items
      // so rather than remove sockets which are not being processed we just don't set the POLLIN flag for them
      for (WFCArray::iterator i = sock_vec.begin(); i != sock_vec.end(); ++i, j++) {
	fds[j].fd = (*i)->fd;
	fds[j].events = 0;
	if (!((*i)->inConnectionThread)) {
	  fds[j].events |= POLLIN;
	}
      }
    }

    // poll with shortish timeout - XXX make timeout configurable
    int res = poll(fds, num_fds, 5);

    if (res < 0) {
      warnlog("poll() system call returned error (%d)", errno);
    }
    else {
      std::lock_guard<std::mutex> lock(sock_vec_mutx);
      for (int i=0; i<num_fds; i++) {
	// set close flag for connections that need closing
	if (fds[i].revents & (POLLHUP | POLLERR | POLLNVAL)) {
	  sock_vec[i]->closeConnection = true;
	}
	// process any connections that have activity
	else if (fds[i].revents & POLLIN) {
	  sock_vec[i]->inConnectionThread = true;
	  sock_vec[i]->q_entry_time = std::chrono::steady_clock::now();
	  p.push(connectionThread, sock_vec[i]);
	}
      }
      // now erase any connections that are done with
      for (WFCArray::iterator i = sock_vec.begin(); i != sock_vec.end();) {
	if ((*i)->closeConnection == true) {
	  // this will implicitly close the socket through the Socket class destructor
	  i = sock_vec.erase(i);
	}
	else
	  ++i;
      }
    }
    delete[] fds;
  }
}

void dnsdistWebserverThread(int sock, const ComboAddress& local, const std::string& password)
{
  warnlog("Webserver launched on %s", local.toStringWithPort());
  auto localACL=g_ACL.getLocal();

  // spin up a thread to do the polling on the connections accepted by this thread
  thread t1(pollThread);
  t1.detach();

  for(;;) {
    try {
      ComboAddress remote(local);
      int fd = SAccept(sock, remote);
      if(!localACL->match(remote)) {
	close(fd);
	continue;
      }
      {
	std::lock_guard<std::mutex> lock(sock_vec_mutx);
	sock_vec.push_back(std::make_shared<WFConnection>(fd, remote, password));
      }
    }
    catch(std::exception& e) {
      errlog("Had an error accepting new webserver connection: %s", e.what());
    }
  }
}
