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

#include "config.h"
#include "wforce.hh"
#include "wforce-replication.hh"
#include <thread>
#include "dolog.hh"
#include "sodcrypto.hh"
#include "base64.hh"
#include "twmap-wrapper.hh"
#include "blackwhitelist.hh"
#include "luastate.hh"
#include "perf-stats.hh"
#include "wforce-web.hh"
#include "common-lua.hh"
#include <fstream>
#include <boost/algorithm/string.hpp>

#ifdef HAVE_GEOIP
#include "wforce-geoip.hh"
#endif
#ifdef HAVE_MMDB
#include "wforce-geoip2.hh"
#endif
#ifdef HAVE_GETDNS
#include "dns_lookup.hh"
#endif

using std::thread;

static vector<std::function<void(void)>>* g_launchWork;

void parseSiblingString(const std::string& str, std::string& ca_str, Sibling::Protocol& proto)
{
  std::vector<std::string> sres;
  boost::split(sres, str, boost::is_any_of(":"));
            
  if (sres.size() > 3) {
    throw WforceException("Malformed sibling string: " + str + "(Format is <host>[:<port>[:<protocol>]]");
  }
  else if (sres.size() == 3) {
    proto = Sibling::stringToProtocol(sres.back());
    sres.pop_back();
    ca_str = boost::join(sres, ":");
  }
  else {
    proto = Sibling::Protocol::UDP;
    ca_str = str;
  }
}

std::vector<std::map<std::string, std::string>> getWLBLKeys(const std::vector<BlackWhiteListEntry>& blv, const char* key_name) {
  std::vector<std::map<std::string, std::string>> ret_vec;
  for (const auto& i : blv) {
    auto key_val = static_cast<std::string>(i.key);
    std::map<std::string, std::string> bl_map = {{key_name, key_val }, {"expiration", boost::posix_time::to_simple_string(i.expiration)}, {"reason", i.reason}};
    ret_vec.emplace_back(bl_map);
  }
  return ret_vec;
}

// lua functions are split into three groups:
// 1) Those which are only applicable as "config/setup" (single global lua state) (they are defined as blank/empty functions otherwise) 
// 2) Those which are only applicable inside the "allow", "report" (and similar) functions, where there are multiple lua states running in different threads, which are defined as empty otherwise
// 3) Those which are applicable to both states
// Functions that are in 2) or 3) MUST be thread-safe. The rest not as they are called at startup.
// We have a single lua config file for historical reasons, hence the somewhat complex structure of this function
// The Lua state and type is passed via "multi_lua" (true means it's one of the multi-lua states used for allow/report etc., false means it's the global lua config state)
vector<std::function<void(void)>> setupLua(bool client, bool multi_lua, LuaContext& c_lua,
					   allow_t& allow_func, 
					   report_t& report_func,
					   reset_t& reset_func,
					   canonicalize_t& canon_func,
					   CustomFuncMap& custom_func_map,
                                           CustomGetFuncMap& custom_get_func_map,
					   const std::string& config)
{
  g_launchWork= new vector<std::function<void(void)>>();

  setupCommonLua(client, multi_lua, c_lua, config);
  
  if (!multi_lua && !client) {
    c_lua.writeFunction("addReportSink", [](const std::string& address) {
	ComboAddress ca;
	try {
	  ca = ComboAddress(address, 4501);
	}
	catch (const WforceException& e) {
          boost::format fmt("%s (%s)\n");
          errlog("addReportSink() error parsing address/port [%s]. Make sure to use IP addresses not hostnames", address);
          g_outputBuffer += (fmt % "addReportSink(): Error parsing address/port. Make sure to use IP addresses not hostnames" % e.reason).str();

	  return;
	}
	g_report_sinks.modify([ca](vector<shared_ptr<Sibling>>& v) {
	    v.push_back(std::make_shared<Sibling>(ca));
	  });
        const std::string notice = "addReportSinks() is deprecated, and will be removed in a future release. Use addNamedReportSink() instead";
        errlog(notice.c_str());
        g_outputBuffer += notice + "\n";
      });
  }
  else {
    c_lua.writeFunction("addReportSink", [](const std::string& address) { });
  }

  if (!multi_lua && !client) {
    c_lua.writeFunction("setReportSinks", [](const vector<pair<int, string>>& parts) {
	vector<shared_ptr<Sibling>> v;
	for(const auto& p : parts) {
	  try {
	    v.push_back(std::make_shared<Sibling>(ComboAddress(p.second, 4501)));
	  }
	  catch (const WforceException& e) {
            boost::format fmt("%s (%s)\n");
            errlog("setReportSinks() error parsing address/port [%s]. Make sure to use IP addresses not hostnames", p.second);
            g_outputBuffer += (fmt % "setReportSinks(): Error parsing address/port. Make sure to use IP addresses not hostnames" % e.reason).str();
	    return;
	  }

	}
	g_report_sinks.setState(v);
        const std::string notice = "setReportSinks() is deprecated, and will be removed in a future release. Use setNamedReportSinks() instead";
        errlog(notice.c_str());
        g_outputBuffer += notice + "\n";
      });
  }
  else {
    c_lua.writeFunction("setReportSinks", [](const vector<pair<int, string>>& parts) { });
  }

  if (!multi_lua && !client) {
    c_lua.writeFunction("addNamedReportSink", [](const std::string& sink_name, const std::string& address) {
	ComboAddress ca;
	try {
	  ca = ComboAddress(address, 4501);
	}
	catch (const WforceException& e) {
          boost::format fmt("%s (%s)\n");
          errlog("addNamedReportSink() error parsing address/port [%s] for report sink [%s]. Make sure to use IP addresses not hostnames", address, sink_name);
          g_outputBuffer += (fmt % "addNamedReportSink(): Error parsing address/port. Make sure to use IP addresses not hostnames" % e.reason).str();
	  return;
	}
	g_named_report_sinks.modify([sink_name, ca](std::map<std::string, std::pair<std::shared_ptr<std::atomic<unsigned int>>, std::vector<std::shared_ptr<Sibling>>>>& m) {
	    const auto& mp = m.find(sink_name);
	    if (mp != m.end())
	      mp->second.second.push_back(std::make_shared<Sibling>(ca));
	    else {
	      vector<shared_ptr<Sibling>> vec;
	      auto atomp = std::make_shared<std::atomic<unsigned int>>(0);
	      vec.push_back(std::make_shared<Sibling>(ca));
	      m.emplace(std::make_pair(sink_name, std::make_pair(atomp, std::move(vec))));
	    }
	  });
      });
  }
  else {
    c_lua.writeFunction("addNamedReportSink", [](const std::string& sink_name,
						 const std::string& address) { });
  }

  if (!multi_lua && !client) {
    c_lua.writeFunction("setNamedReportSinks", [](const std::string& sink_name, const vector<pair<int, string>>& parts) {
	g_named_report_sinks.modify([sink_name, parts](std::map<std::string, std::pair<std::shared_ptr<std::atomic<unsigned int>>, std::vector<std::shared_ptr<Sibling>>>>& m) {
	    vector<shared_ptr<Sibling>> v;
	    for(const auto& p : parts) {
	      try {
		v.push_back(std::make_shared<Sibling>(ComboAddress(p.second, 4501)));
	      }
	      catch (const WforceException& e) {
                boost::format fmt("%s (%s)\n");
                errlog("setNamedReportSinks() error parsing address/port [%s] for report sink [%s]. Make sure to use IP addresses not hostnames", p.second, sink_name);
                g_outputBuffer += (fmt % "setNamedReportSinks(): Error parsing address/port. Make sure to use IP addresses not hostnames" % e.reason).str();
		return;
	      }
	    }
	    const auto& mp = m.find(sink_name);
	    if (mp != m.end())
	      mp->second.second = v;
	    else {
	      auto atomp = std::make_shared<std::atomic<unsigned int>>(0);
	      m.emplace(std::make_pair(sink_name, std::make_pair(atomp, std::move(v))));
	    }
	  });
      });
  }
  else {
    c_lua.writeFunction("setNamedReportSinks", [](const std::string& sink_name, const vector<pair<int, string>>& parts) { });
  }

  if (!multi_lua && !client) {
    c_lua.writeFunction("addSyncHost", [](const std::string& address, const std::string password, const std::string& sync_address, const std::string& webserver_address) {
	try {
          g_sync_data.sync_hosts.push_back(std::make_pair(ComboAddress(address, 8084), password));
          g_sync_data.sibling_listen_addr = ComboAddress(sync_address, 4001);
          g_sync_data.webserver_listen_addr = ComboAddress(webserver_address, 8084);
	}
	catch (const WforceException& e) {
          const std::string errstr = (boost::format("%s (%s)\n") % "addSyncHost(): Error parsing address/port. Make sure to use IP addresses not hostnames" % e.reason).str();
          errlog(errstr.c_str());
          g_outputBuffer += errstr;
	  return;
	}
      });
  }
  else {
    c_lua.writeFunction("addSyncHost", [](const std::string& address, const std::string password, const std::string& sync_address, const std::string& webserver_address) {});
  }

  if (!multi_lua) {
    c_lua.writeFunction("setMinSyncHostUptime", [](unsigned int uptime) {
        g_sync_data.min_sync_host_uptime = uptime;
      });
  }
  else {
    c_lua.writeFunction("setMinSyncHostUptime", [](unsigned int uptime) {});
  }

  if (!multi_lua) {
    c_lua.writeFunction("setMaxSiblingQueueSize", [](unsigned int size) {
        setMaxSiblingQueueSize(size);
      });
  }
  else {
    c_lua.writeFunction("setMaxSiblingQueueSize", [](unsigned int size) {});
  }

  
  if (!multi_lua && !client) {
    c_lua.writeFunction("addSibling", [](const std::string& address) {
	ComboAddress ca;
        std::string ca_str;
        Sibling::Protocol proto;
        parseSiblingString(address, ca_str, proto);
	try {
	  ca = ComboAddress(ca_str, 4001);
	}
	catch (const WforceException& e) {
          const std::string errstr = (boost::format("%s [%s]. %s (%s)\n") % "addSibling() error parsing address/port" % address % "Make sure to use IP addresses not hostnames" % e.reason).str();
          errlog(errstr.c_str());
          g_outputBuffer += errstr;
	  return;
	}
	g_siblings.modify([ca, proto](vector<shared_ptr<Sibling>>& v) { v.push_back(std::make_shared<Sibling>(ca, proto)); });
      });
  }
  else {
    c_lua.writeFunction("addSibling", [](const std::string& address) { });
  }

  if (!multi_lua && !client) {
    c_lua.writeFunction("setSiblings", [](const vector<pair<int, string>>& parts) {
        vector<shared_ptr<Sibling>> v;
	for(const auto& p : parts) {
          try {
            std::string ca_string;
            Sibling::Protocol proto;

            parseSiblingString(p.second, ca_string, proto);
            v.push_back(std::make_shared<Sibling>(ComboAddress(ca_string, 4001), proto));
          }
          catch (const WforceException& e) {
            const std::string errstr = (boost::format("%s [%s]. %s (%s)\n") % "addSibling() error parsing address/port" % p.second % "Make sure to use IP addresses not hostnames" % e.reason).str();
            errlog(errstr.c_str());
            g_outputBuffer += errstr;
          }
	}
	g_siblings.setState(v);
      });
  }
  else {
    c_lua.writeFunction("setSiblings", [](const vector<pair<int, string>>& parts) { });
  }

  if (!multi_lua && !client) {
    c_lua.writeFunction("siblingListener", [](const std::string& address) {
	ComboAddress ca;
	try {
	  ca = ComboAddress(address, 4001);
	}
	catch (const WforceException& e) {
          const std::string errstr = (boost::format("%s [%s]. %s (%s)\n") % "addSibling() error parsing address/port" % address % "Make sure to use IP addresses not hostnames" % e.reason).str();
          errlog(errstr.c_str());
          g_outputBuffer += errstr;
	  return;
	}
	auto launch = [ca]() {
          auto siblings = g_siblings.getLocal();

          for(auto& s : *siblings) {
            s->checkIgnoreSelf(ca);
          }
          
          thread t1(receiveReplicationOperations, ca);
	  t1.detach();
	  thread t2(receiveReplicationOperationsTCP, ca);
	  t2.detach();
	};
	if(g_launchWork)
	  g_launchWork->push_back(launch);
	else
	  launch();
      });
  }
  else {
    c_lua.writeFunction("siblingListener", [](const std::string& address) { });
  }

  c_lua.writeFunction("shutdown", []() { _exit(0);} );

  if (!multi_lua) {
    c_lua.writeFunction("webserver", [client](const std::string& address, const std::string& password) {
	if(client)
	  return;
	ComboAddress local;
	try {
	  local = ComboAddress(address);
	}
	catch (const WforceException& e) {
          errlog("webserver() error parsing address/port [%s]. Make sure to use IP addresses not hostnames", address);
          return;
	}
        g_sync_data.webserver_password = password;
	try {
	  int sock = socket(local.sin4.sin_family, SOCK_STREAM, 0);
	  SSetsockopt(sock, SOL_SOCKET, SO_REUSEADDR, 1);
	  SBind(sock, local);
	  SListen(sock, 1024);
	  auto launch=[sock, local, password]() {
	    thread t(WforceWebserver::start, sock, local, password, &g_webserver);
	    t.detach();
	  };
	  if(g_launchWork) 
	    g_launchWork->push_back(launch);
	  else
	    launch();	    
	}
	catch(std::exception& e) {
	  errlog("Unable to bind to webserver socket on %s: %s", local.toStringWithPort(), e.what());
	}

      });
  }
  else {
    c_lua.writeFunction("webserver", [](const std::string& address, const std::string& password) { });
  }

  if (!multi_lua) {
    c_lua.writeFunction("controlSocket", [client](const std::string& str) {
	ComboAddress local;
	try {
	  local = ComboAddress(str, 4004);
	}
	catch (const WforceException& e) {
	  errlog("controlSocket() error parsing address/port [%s]. Make sure to use IP addresses not hostnames", str);
	  return;
	}
	if(client) {
	  g_serverControl = local;
	  return;
	}
      
	try {
	  int sock = socket(local.sin4.sin_family, SOCK_STREAM, 0);
	  SSetsockopt(sock, SOL_SOCKET, SO_REUSEADDR, 1);
	  SBind(sock, local);
	  SListen(sock, 5);
	  auto launch=[sock, local]() {
	    thread t(controlThread, sock, local);
	    t.detach();
	  };
	  if(g_launchWork) 
	    g_launchWork->push_back(launch);
	  else
	    launch();
	    
	}
	catch(std::exception& e) {
	  errlog("Unable to bind to control socket on %s: %s", local.toStringWithPort(), e.what());
	}
      });
  }
  else {
    c_lua.writeFunction("controlSocket", [](const std::string& str) { });
  }

  if (!multi_lua) {
    c_lua.writeFunction("stats", []() {
	boost::format fmt("%d reports, %d allow-queries (%d denies)\n");
	g_outputBuffer = (fmt % g_stats.reports % g_stats.allows % g_stats.denieds).str();  
      });
  }
  else {
    c_lua.writeFunction("stats", []() { });
  }

  if (!multi_lua) {
    c_lua.writeFunction("siblings", []() {
        auto siblings = g_siblings.getCopy();
        boost::format fmt("%-35s %-15d %-14d %-15d %-14d   %s\n");
        g_outputBuffer= (fmt % "Address" % "Send Successes" % "Send Failures" % "Rcv Successes" % "Rcv Failures" % "Note").str();
        for(const auto& s : siblings) {
          std::string addr =  s->rem.toStringWithPort() + " " + Sibling::protocolToString(s->proto);
          g_outputBuffer += (fmt % addr % s->success % s->failures % s->rcvd_success % s->rcvd_fail % (s->d_ignoreself ? "Self" : "") ).str();
        }
      });
  } 
  else {
    c_lua.writeFunction("siblings", []() { });
  }	

  if (!multi_lua) {
    c_lua.writeFunction("showReportSinks", []() {
	auto rsinks = g_report_sinks.getCopy();
	boost::format fmt("%-35s %-10d %-9d\n");
	g_outputBuffer= (fmt % "Address" % "Successes" % "Failures").str();
	for(const auto& s : rsinks)
	  g_outputBuffer += (fmt % s->rem.toStringWithPort() % s->success % s->failures).str();
      });
  }
  else {
    c_lua.writeFunction("showReportSinks", []() { });
  }

  if (!multi_lua) {
    c_lua.writeFunction("showNamedReportSinks", []() {
	auto rsinks = g_named_report_sinks.getCopy();
	boost::format fmt("%-15s %-35s %-10d %-9d\n");
	g_outputBuffer= (fmt % "Name" % "Address" % "Successes" % "Failures").str();
	for(const auto& s : rsinks)
	  for (const auto& v : s.second.second)
	    g_outputBuffer += (fmt % s.first % v->rem.toStringWithPort() % v->success % v->failures).str();
      });
  }
  else {
    c_lua.writeFunction("showNamedReportSinks", []() { });
  }
  
  if (!multi_lua) {
    c_lua.writeFunction("setNumSiblingThreads", [](int numThreads) {
	// the number of threads used to process sibling reports
	g_num_sibling_threads = numThreads;
      });
  }
  else {
    c_lua.writeFunction("setNumSiblingThreads", [](int numThreads) { });    
  }
  
#ifdef HAVE_GETDNS
  if (!multi_lua) {
    c_lua.writeFunction("newDNSResolver", [](const std::string& name) { 
	std::lock_guard<std::mutex> lock(resolv_mutx);
	resolvMap.insert(std::make_pair(name, std::make_shared<WFResolver>()));
      });
  }
  else {
    c_lua.writeFunction("newDNSResolver", [](const std::string& name) { });
  }
  c_lua.registerFunction("addResolver", &WFResolver::add_resolver);
  c_lua.registerFunction("setRequestTimeout", &WFResolver::set_request_timeout);
  c_lua.registerFunction("setNumContexts", &WFResolver::set_num_contexts);
  c_lua.writeFunction("getDNSResolver", [](const std::string& name) {
      std::lock_guard<std::mutex> lock(resolv_mutx);
      auto it = resolvMap.find(name);
      if (it != resolvMap.end())
	return it->second; // copy
      else
	return std::make_shared<WFResolver>(); // copy
    });
  c_lua.registerFunction("lookupAddrByName", &WFResolver::lookup_address_by_name);
  c_lua.registerFunction("lookupNameByAddr", &WFResolver::lookup_name_by_address);
  c_lua.registerFunction("lookupRBL", &WFResolver::lookupRBL);
  // The following "show.." functions are mainly for regression tests
  if (!multi_lua) {
    c_lua.writeFunction("showAddrByName", [](std::shared_ptr<WFResolver> resolvp, string name) {
	std::vector<std::string> retvec = resolvp->lookup_address_by_name(name, 1);
	boost::format fmt("%s %s\n");
	for (const auto s : retvec) {
	  g_outputBuffer += (fmt % name % s).str();
	}
      });
  } else {
    c_lua.writeFunction("showAddrByName", [](std::shared_ptr<WFResolver> resolvp, string name) { });
  }
  
  if (!multi_lua) {
    c_lua.writeFunction("showNameByAddr", [](std::shared_ptr<WFResolver> resolvp, ComboAddress address) {
	std::vector<std::string> retvec = resolvp->lookup_name_by_address(address, 1);
	boost::format fmt("%s %s\n");
	for (const auto s : retvec) {
	  g_outputBuffer += (fmt % address.toString() % s).str();
	}
      });
  }
  else {
    c_lua.writeFunction("showNameByAddr", [](std::shared_ptr<WFResolver> resolvp, ComboAddress address) { });
  }
  
  if (!multi_lua) {
    c_lua.writeFunction("showRBL", [](std::shared_ptr<WFResolver> resolvp, ComboAddress address, string rblname) {
	std::vector<std::string> retvec = resolvp->lookupRBL(address, rblname, 1);
	boost::format fmt("%s\n");
	for (const auto s : retvec) {
	  g_outputBuffer += (fmt % s).str();
	}	
      });	
  }
  else {
    c_lua.writeFunction("showRBL", [](std::shared_ptr<WFResolver> resolvp, ComboAddress address, string rblname) { });    
  }
#endif // HAVE_GETDNS

  if (!multi_lua) {
    c_lua.writeFunction("newStringStatsDB", [](const std::string& name, int window_size, int num_windows, const std::vector<pair<std::string, std::string>>& fmvec) {
	auto twsdbw = TWStringStatsDBWrapper(name, window_size, num_windows, fmvec);
	// register this statsDB in a map for retrieval later
	std::lock_guard<std::mutex> lock(dbMap_mutx);
	dbMap.emplace(std::make_pair(name, twsdbw));
      });
  }
  else {
    c_lua.writeFunction("newStringStatsDB", [](const std::string& name, int window_size, int num_windows, const std::vector<pair<std::string, std::string>>& fmvec) { });
  }

  c_lua.writeFunction("getStringStatsDB", [](const std::string& name) {
      std::lock_guard<std::mutex> lock(dbMap_mutx);
      auto it = dbMap.find(name);
      if (it != dbMap.end())
	return it->second; // copy
      else {
	warnlog("getStringStatsDB(): could not find DB %s", name);
	return TWStringStatsDBWrapper("none", 1, 1);
      }
    });

  if (!multi_lua) {
    c_lua.writeFunction("showStringStatsDB", []() {
	std::lock_guard<std::mutex> lock(dbMap_mutx);
	boost::format fmt("%-20.20d %-5.5s %-11.11d %-9d %-9d %-16.16s %-s\n");
	g_outputBuffer= (fmt % "DB Name" % "Repl?" % "Win Size/No" % "Max Size" % "Cur Size" % "Field Name" % "Type").str();
	for (auto& i : dbMap) {
	  const FieldMap fields = i.second.getFields();
	  for (auto f=fields.begin(); f!=fields.end(); ++f) {
	    if (f == fields.begin()) {
	      std::string win = std::to_string(i.second.windowSize()) + "/" + std::to_string(i.second.numWindows());
	      g_outputBuffer += (fmt % i.first % (i.second.getReplicationStatus() ? "yes" : "no") % win % i.second.get_max_size() % i.second.get_size() % f->first % f->second).str();
	    }
	    else {
	      g_outputBuffer += (fmt % "" % "" % "" % "" % "" % f->first % f->second).str();
	    }
	  }
	}
      });
  }
  else {
    c_lua.writeFunction("showStringStatsDB", []() { });
  }
  
  c_lua.registerFunction("twAdd", &TWStringStatsDBWrapper::add);
  c_lua.registerFunction("twSub", &TWStringStatsDBWrapper::sub);
  c_lua.registerFunction("twGet", &TWStringStatsDBWrapper::get);
  c_lua.registerFunction("twGetCurrent", &TWStringStatsDBWrapper::get_current);
  c_lua.registerFunction("twGetWindows", &TWStringStatsDBWrapper::get_windows);
  c_lua.registerFunction("twSetv4Prefix", &TWStringStatsDBWrapper::setv4Prefix);
  c_lua.registerFunction("twSetv6Prefix", &TWStringStatsDBWrapper::setv6Prefix);
  c_lua.registerFunction("twGetSize", &TWStringStatsDBWrapper::get_size);
  c_lua.registerFunction("twSetMaxSize", &TWStringStatsDBWrapper::set_size_soft);
  c_lua.registerFunction("twReset", &TWStringStatsDBWrapper::reset);
  c_lua.registerFunction("twResetField", &TWStringStatsDBWrapper::resetField);
  c_lua.registerFunction("twEnableReplication", &TWStringStatsDBWrapper::enableReplication);
  c_lua.registerFunction("twGetName", &TWStringStatsDBWrapper::getDBName);
  c_lua.registerFunction("twSetExpireSleep", &TWStringStatsDBWrapper::set_expire_sleep);
  // Blacklists
  if (!multi_lua) {
    c_lua.writeFunction("disableBuiltinBlacklists", []() {
        g_builtin_bl_enabled = false;
      });
  }
  else {
    c_lua.writeFunction("disableBuiltinBlacklists", []() {});
  }
  
  if (!client) {
    c_lua.writeFunction("blacklistNetmask", [](const Netmask& nm, unsigned int seconds, const std::string& reason) {
        g_bl_db.addEntry(nm, seconds, reason);
      });
  
    c_lua.writeFunction("blacklistIP", [](const ComboAddress& ca, unsigned int seconds, const std::string& reason) {
        g_bl_db.addEntry(ca, seconds, reason);
      });

    c_lua.writeFunction("blacklistLogin", [](const std::string& login, unsigned int seconds, const std::string& reason) {
        g_bl_db.addEntry(login, seconds, reason);
      });

    c_lua.writeFunction("blacklistIPLogin", [](const ComboAddress& ca, const std::string& login, unsigned int seconds, const std::string& reason) {
        g_bl_db.addEntry(ca, login, seconds, reason);
      });

    c_lua.writeFunction("unblacklistNetmask", [](const Netmask& nm) {
        g_bl_db.deleteEntry(nm);
      });
  
    c_lua.writeFunction("unblacklistIP", [](const ComboAddress& ca) {
        g_bl_db.deleteEntry(ca);
      });

    c_lua.writeFunction("unblacklistLogin", [](const std::string& login) {
        g_bl_db.deleteEntry(login);
      });

    c_lua.writeFunction("unblacklistIPLogin", [](const ComboAddress& ca, const std::string& login) {
        g_bl_db.deleteEntry(ca, login);
      });

    c_lua.writeFunction("checkBlacklistIP", [](const ComboAddress& ca) {
        return g_bl_db.checkEntry(ca);
      });

    c_lua.writeFunction("checkBlacklistLogin", [](const std::string& login) {
        return g_bl_db.checkEntry(login);
      });

    c_lua.writeFunction("checkBlacklistIPLogin", [](const ComboAddress& ca, const std::string& login) {
        return g_bl_db.checkEntry(ca, login);
      });

    c_lua.writeFunction("getIPBlacklist", []() {
        auto bl = g_bl_db.getIPEntries();
        return getWLBLKeys(bl, "ip");
      });
    c_lua.writeFunction("getLoginBlacklist", []() {
        auto bl = g_bl_db.getLoginEntries();
        return getWLBLKeys(bl, "login");
      });
    c_lua.writeFunction("getIPLoginBlacklist", []() {
        auto bl = g_bl_db.getIPLoginEntries();
        return getWLBLKeys(bl, "iplogin");
      });
    c_lua.writeFunction("getBlacklistIPRetMsg", []() {
        return g_bl_db.getIPRetMsg();
      });
    c_lua.writeFunction("getBlacklistLoginRetMsg", []() {
        return g_bl_db.getLoginRetMsg();
      });
    c_lua.writeFunction("getBlacklistIPLoginRetMsg", []() {
        return g_bl_db.getIPLoginRetMsg();
      });
  }
  else {
    c_lua.writeFunction("blacklistNetmask", [](const Netmask& nm, unsigned int seconds, const std::string& reason) {
      });
  
    c_lua.writeFunction("blacklistIP", [](const ComboAddress& ca, unsigned int seconds, const std::string& reason) {
      });

    c_lua.writeFunction("blacklistLogin", [](const std::string& login, unsigned int seconds, const std::string& reason) {
      });

    c_lua.writeFunction("blacklistIPLogin", [](const ComboAddress& ca, const std::string& login, unsigned int seconds, const std::string& reason) {
      });

    c_lua.writeFunction("unblacklistNetmask", [](const Netmask& nm) {
      });
  
    c_lua.writeFunction("unblacklistIP", [](const ComboAddress& ca) {
      });

    c_lua.writeFunction("unblacklistLogin", [](const std::string& login) {
      });

    c_lua.writeFunction("unblacklistIPLogin", [](const ComboAddress& ca) {
      });

    c_lua.writeFunction("checkBlacklistIP", [](const ComboAddress& ca) {});

    c_lua.writeFunction("checkBlacklistLogin", [](const std::string& login) {});

    c_lua.writeFunction("checkBlacklistIPLogin", [](const ComboAddress& ca, const std::string& login) {});

    c_lua.writeFunction("getIPBlacklist", []() {});
    c_lua.writeFunction("getLoginBlacklist", []() {});
    c_lua.writeFunction("getIPLoginBlacklist", []() {});
    c_lua.writeFunction("getBlacklistIPRetMsg", []() {});
    c_lua.writeFunction("getBlacklistLoginRetMsg", []() {});
    c_lua.writeFunction("getBlacklistIPLoginRetMsg", []() {});
  }
  
  if (!multi_lua) {
    c_lua.writeFunction("blacklistPersistDB", [](const std::string& ip, unsigned int port) {
	g_bl_db.makePersistent(ip, port);
      });
    c_lua.writeFunction("blacklistPersistReplicated", []() { g_bl_db.persistReplicated(); });
    c_lua.writeFunction("blacklistPersistConnectTimeout", [](int timeout_secs) { g_bl_db.setConnectTimeout(timeout_secs); });
    c_lua.writeFunction("setBlacklistIPRetMsg", [](const std::string& msg) {
        g_bl_db.setIPRetMsg(msg);
      });
    c_lua.writeFunction("setBlacklistLoginRetMsg", [](const std::string& msg) {
        g_bl_db.setLoginRetMsg(msg);
      });
    c_lua.writeFunction("setBlacklistIPLoginRetMsg", [](const std::string& msg) {
        g_bl_db.setIPLoginRetMsg(msg);
      });
  }
  else {
    c_lua.writeFunction("blacklistPersistDB", [](const std::string& ip, unsigned int port) {});
    c_lua.writeFunction("blacklistPersistReplicated", []() {});
    c_lua.writeFunction("blacklistPersistConnectTimeout", [](int timeout_secs) {});
    c_lua.writeFunction("setBlacklistIPRetMsg", [](const std::string& msg) {});
    c_lua.writeFunction("setBlacklistLoginRetMsg", [](const std::string& msg) {});
    c_lua.writeFunction("setBlacklistIPLoginRetMsg", [](const std::string& msg) {});
  }
  // End blacklists

  // Whitelists
  if (!multi_lua) {
    c_lua.writeFunction("disableBuiltinWhitelists", []() {
        g_builtin_wl_enabled = false;
      });
  }
  else {
    c_lua.writeFunction("disableBuiltinWhitelists", []() {});
  }
  
  if (!client) {
    c_lua.writeFunction("whitelistNetmask", [](const Netmask& nm, unsigned int seconds, const std::string& reason) {
        g_wl_db.addEntry(nm, seconds, reason);
      });
  
    c_lua.writeFunction("whitelistIP", [](const ComboAddress& ca, unsigned int seconds, const std::string& reason) {
        g_wl_db.addEntry(ca, seconds, reason);
      });

    c_lua.writeFunction("whitelistLogin", [](const std::string& login, unsigned int seconds, const std::string& reason) {
        g_wl_db.addEntry(login, seconds, reason);
      });

    c_lua.writeFunction("whitelistIPLogin", [](const ComboAddress& ca, const std::string& login, unsigned int seconds, const std::string& reason) {
        g_wl_db.addEntry(ca, login, seconds, reason);
      });

    c_lua.writeFunction("unwhitelistNetmask", [](const Netmask& nm) {
        g_wl_db.deleteEntry(nm);
      });
  
    c_lua.writeFunction("unwhitelistIP", [](const ComboAddress& ca) {
        g_wl_db.deleteEntry(ca);
      });

    c_lua.writeFunction("unwhitelistLogin", [](const std::string& login) {
        g_wl_db.deleteEntry(login);
      });

    c_lua.writeFunction("unwhitelistIPLogin", [](const ComboAddress& ca, const std::string& login) {
        g_wl_db.deleteEntry(ca, login);
      });

    c_lua.writeFunction("checkWhitelistIP", [](const ComboAddress& ca) {
        return g_wl_db.checkEntry(ca);
      });

    c_lua.writeFunction("checkWhitelistLogin", [](const std::string& login) {
        return g_wl_db.checkEntry(login);
      });

    c_lua.writeFunction("checkWhitelistIPLogin", [](const ComboAddress& ca, const std::string& login) {
        return g_wl_db.checkEntry(ca, login);
      });

    c_lua.writeFunction("getIPWhitelist", []() {
        auto wl = g_wl_db.getIPEntries();
        return getWLBLKeys(wl, "ip");
      });
    c_lua.writeFunction("getLoginWhitelist", []() {
        auto wl = g_wl_db.getLoginEntries();
        return getWLBLKeys(wl, "login");
      });
    c_lua.writeFunction("getIPLoginWhitelist", []() {
        auto wl = g_wl_db.getIPLoginEntries();
        return getWLBLKeys(wl, "iplogin");
      });

  }
  else {
    c_lua.writeFunction("whitelistNetmask", [](const Netmask& nm, unsigned int seconds, const std::string& reason) {
      });
  
    c_lua.writeFunction("whitelistIP", [](const ComboAddress& ca, unsigned int seconds, const std::string& reason) {
      });

    c_lua.writeFunction("whitelistLogin", [](const std::string& login, unsigned int seconds, const std::string& reason) {
      });

    c_lua.writeFunction("whitelistIPLogin", [](const ComboAddress& ca, const std::string& login, unsigned int seconds, const std::string& reason) {
      });

    c_lua.writeFunction("unwhitelistNetmask", [](const Netmask& nm) {
      });
  
    c_lua.writeFunction("unwhitelistIP", [](const ComboAddress& ca) {
      });

    c_lua.writeFunction("unwhitelistLogin", [](const std::string& login) {
      });

    c_lua.writeFunction("unwhitelistIPLogin", [](const ComboAddress& ca) {
      });

    c_lua.writeFunction("checkWhitelistIP", [](const ComboAddress& ca) {});

    c_lua.writeFunction("checkWhitelistLogin", [](const std::string& login) {});

    c_lua.writeFunction("checkWhitelistIPLogin", [](const ComboAddress& ca, const std::string& login) {});

    c_lua.writeFunction("getIPWhitelist", []() {});
    c_lua.writeFunction("getLoginWhitelist", []() {});
    c_lua.writeFunction("getIPLoginWhitelist", []() {});
  }
  
  if (!multi_lua) {
    c_lua.writeFunction("whitelistPersistDB", [](const std::string& ip, unsigned int port) {
	g_wl_db.makePersistent(ip, port);
      });
    c_lua.writeFunction("whitelistPersistReplicated", []() { g_wl_db.persistReplicated(); });
    c_lua.writeFunction("whitelistPersistConnectTimeout", [](int timeout_secs) { g_wl_db.setConnectTimeout(timeout_secs); });
  }
  else {
    c_lua.writeFunction("whitelistPersistDB", [](const std::string& ip, unsigned int port) {});
    c_lua.writeFunction("whitelistPersistReplicated", []() {});
    c_lua.writeFunction("whitelistPersistConnectTimeout", [](int timeout_secs) {});
  }
  // End whitelists
  
  if (multi_lua) {
    c_lua.writeFunction("setAllow", [&allow_func](allow_t func) { allow_func=func;});
  }
  else {
    c_lua.writeFunction("setAllow", [](allow_t func) { });    
  }

  if (multi_lua) {
    c_lua.writeFunction("setReport", [&report_func](report_t func) { report_func=func;});
  }
  else {
    c_lua.writeFunction("setReport", [](report_t func) { });
  }

  if (multi_lua) {
    c_lua.writeFunction("setReset", [&reset_func](reset_t func) { reset_func=func;});
  }
  else {
    c_lua.writeFunction("setReset", [](reset_t func) { });
  }

  if (multi_lua) {
    c_lua.writeFunction("setCanonicalize", [&canon_func](canonicalize_t func) { canon_func=func;});
  }
  else {
    c_lua.writeFunction("setCanonicalize", [](canonicalize_t func) { });
  }

  if (!multi_lua) {
    c_lua.writeFunction("setVerboseAllowLog", []() {
	g_allowlog_verbose = true;
      });
  }
  else {
    c_lua.writeFunction("setVerboseAllowLog", []() { });
  }

  c_lua.registerMember("attrs", &CustomFuncArgs::attrs);
  c_lua.registerMember("attrs_mv", &CustomFuncArgs::attrs_mv);

  c_lua.writeFunction("setCustomEndpoint", [&custom_func_map, multi_lua, client](const std::string& f_name, bool reportSink, custom_func_t func) {
      CustomFuncMapObject cobj;
      cobj.c_func = func;
      cobj.c_reportSink = reportSink;
      custom_func_map.insert(std::make_pair(f_name, cobj));
      if (!multi_lua && !client) {
        addCommandStat(f_name);
	// register a webserver command
	g_webserver.registerFunc(f_name, HTTPVerb::POST, WforceWSFunc(parseCustomCmd));
	noticelog("Registering custom POST endpoint [%s]", f_name);
      }
    });

  c_lua.writeFunction("setCustomGetEndpoint", [&custom_get_func_map, multi_lua, client](const std::string& f_name,  custom_get_func_t func) {
      custom_get_func_map.insert(std::make_pair(f_name, func));
      if (!multi_lua && !client) {
        addCommandStat(f_name+"_Get");
	// register a webserver command
	g_webserver.registerFunc(f_name, HTTPVerb::GET, WforceWSFunc(parseCustomGetCmd, "text/plain"));
	noticelog("Registering custom GET endpoint [%s]", f_name);
      }
    });
  
  if (!multi_lua) {
    c_lua.writeFunction("showCustomEndpoints", []() {
	boost::format fmt("%-30.30s %-5.5s %-s \n");
	g_outputBuffer = (fmt % "Custom Endpoint" % "Type" % "Send to Report Sink?").str();
	for (const auto& i : g_custom_func_map) {
	  std::string reportStr = i.second.c_reportSink == true ? "true" : "false";
	  g_outputBuffer += (fmt % i.first % "POST" % reportStr).str();
	}
	for (const auto& i : g_custom_get_func_map) {
	  g_outputBuffer += (fmt % i.first % "GET" % "false").str();
	}
      });
  }
  else {
    c_lua.writeFunction("showCustomEndpoints", []() { });
  }
    
  if (!multi_lua) {
    c_lua.writeFunction("showWebHooks", []() {
	auto webhooks = g_webhook_db.getWebHooks();
	boost::format fmt("%-9d %-9d %-9d %-45.45s %-s\n");
	g_outputBuffer= (fmt % "ID" % "Successes" % "Failures" % "URL" % "Events").str();
	for(const auto& i : webhooks) {
	  if (auto is = i.lock())
	    if (is)
	      g_outputBuffer += (fmt % is->getID() % is->getSuccess() % is->getFailed() % is->getConfigKey("url") % is->getEventsStr()).str();
	}
      });
  }
  else {
    c_lua.writeFunction("showWebHooks", []() { });
  }
  
  if (!(multi_lua || client)) {
    c_lua.writeFunction("addWebHook", [](const std::vector<std::pair<int, std::string>>& events_vec, const std::vector<std::pair<std::string, std::string>>& ck_vec) {
        std::string err;
        WHEvents events;
        WHConfigMap config_keys;

        auto id = g_webhook_db.getNewID();
        for (const auto& ev : events_vec) {
          events.push_back(ev.second);
        }
        for (const auto& ck : ck_vec) {
          config_keys.insert(ck);
        }
        auto ret = g_webhook_db.addWebHook(WebHook(id, events, true, config_keys), err);
        if (ret != true) {
          errlog("Registering webhook id=%d from Lua failed [%s]", id, err);
        }
      });
  }
  else {
    c_lua.writeFunction("addWebHook", [](const std::vector<std::pair<int, std::string>>& events_vec, const std::vector<std::pair<std::string, std::string>>& ck_vec) { });
  }

  if (!multi_lua) {
    c_lua.writeFunction("showVersion", []() {
	g_outputBuffer = "wforce " + std::string(VERSION) + "\n";
      });
  }
  else {
    c_lua.writeFunction("showVersion", []() { });
  }
  
  if (!multi_lua) {
    c_lua.writeFunction("testCrypto", [](string testmsg)
			{
			  try {
			    SodiumNonce sn, sn2;
			    sn.init();
			    sn2=sn;
			    string encrypted = sodEncryptSym(testmsg, g_key, sn);
			    string decrypted = sodDecryptSym(encrypted, g_key, sn2);
       
			    sn.increment();
			    sn2.increment();

			    encrypted = sodEncryptSym(testmsg, g_key, sn);
			    decrypted = sodDecryptSym(encrypted, g_key, sn2);

			    if(testmsg == decrypted)
			      g_outputBuffer="Everything is ok!\n";
			    else
			      g_outputBuffer="Crypto failed..\n";
       
			  }
			  catch(...) {
			    g_outputBuffer="Crypto failed..\n";
			  }});
  }
  else {
    c_lua.writeFunction("testCrypto", [](string testmsg) {});
  }

  if (!multi_lua) {
    c_lua.writeFunction("setHLLBits", [](unsigned int nbits) {
        TWStatsMemberHLL::setNumBits(nbits);
      });
  }
  else {
    c_lua.writeFunction("setHLLBits", [](unsigned int nbits) { });
  }

  if (!multi_lua) {
    c_lua.writeFunction("setCountMinBits", [](float gamma, float eps) {
        TWStatsMemberCountMin::setGamma(gamma);
        TWStatsMemberCountMin::setEPS(eps);
      });
  }
  else {
    c_lua.writeFunction("setCountMinBits", [](float gamma, float eps) { });
  }
  
  std::ifstream ifs(config);
  if(!ifs) 
    warnlog("Unable to read configuration from '%s'", config);
  else if (!multi_lua)
    infolog("Read configuration from '%s'", config);

  c_lua.executeCode(ifs);
  if (!multi_lua) {
    auto ret=*g_launchWork;
    delete g_launchWork;
    g_launchWork=0;
    return ret;
  }
  else {
    return vector<std::function<void(void)>>();
  }
}
