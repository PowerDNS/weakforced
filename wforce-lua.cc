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
#include <thread>
#include "dolog.hh"
#include "sodcrypto.hh"
#include "base64.hh"
#include "twmap-wrapper.hh"
#include "blacklist.hh"
#include "luastate.hh"
#include "perf-stats.hh"
#include "wforce-web.hh"
#include <fstream>

#ifdef HAVE_GEOIP
#include "wforce-geoip.hh"
#endif
#ifdef HAVE_GETDNS
#include "dns_lookup.hh"
#endif

using std::thread;

static vector<std::function<void(void)>>* g_launchWork;

// lua functions are split into three groups:
// 1) Those which are only applicable as "config/setup" (single global lua state) (they are defined as blank/empty functions otherwise) 
// 2) Those which are only applicable inside the "allow" or "report" functions (multiple lua states running in different threads), which are defined as blank/empty otherwise
// 3) Those which are applicable to both states
// Functions that are in 2) or 3) MUST be thread-safe. The rest not as they are called at startup.
// We have a single lua config file for historical reasons, hence the somewhat complex structure of this function
// The Lua state and type is passed via "allow_report" (true means it's one of the multiple states used for allow/report, false means it's the global lua config state) 
vector<std::function<void(void)>> setupLua(bool client, bool allow_report, LuaContext& c_lua,  
					   allow_t& allow_func, 
					   report_t& report_func,
					   reset_t& reset_func,
					   canonicalize_t& canon_func,
					   CustomFuncMap& custom_func_map,
					   const std::string& config)
{
  g_launchWork= new vector<std::function<void(void)>>();
  
  if (!allow_report) {
    c_lua.writeFunction("addACL", [](const std::string& domain) {
	g_webserver.addACL(domain);
      });
  }
  else { // empty function for allow/report - this stops parsing errors or weirdness
    c_lua.writeFunction("addACL", [](const std::string& domain) { });
  }

  if (!allow_report) {
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

  if (!allow_report) {
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

  if (!allow_report) {
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

  if (!allow_report) {
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

  
  if (!allow_report) {
    c_lua.writeFunction("addSibling", [](const std::string& address) {
	ComboAddress ca;
	try {
	  ca = ComboAddress(address, 4001);
	}
	catch (const WforceException& e) {
          boost::format fmt("%s (%s)\n");
          errlog("addSibling() error parsing address/port [%s]. Make sure to use IP addresses not hostnames", address);
          g_outputBuffer += (fmt % "addSibling(): Error parsing address/port. Make sure to use IP addresses not hostnames" % e.reason).str();
	  return;
	}
	g_siblings.modify([ca](vector<shared_ptr<Sibling>>& v) { v.push_back(std::make_shared<Sibling>(ca)); });
      });
  }
  else {
    c_lua.writeFunction("addSibling", [](const std::string& address) { });
  }

  if (!allow_report) {
    c_lua.writeFunction("setSiblings", [](const vector<pair<int, string>>& parts) {
	vector<shared_ptr<Sibling>> v;
	for(const auto& p : parts) {
          try {
            v.push_back(std::make_shared<Sibling>(ComboAddress(p.second, 4001)));
          }
          catch (const WforceException& e) {
            boost::format fmt("%s [%s] %s (%s)\n");
            errlog("setSiblings() error parsing address/port [%s]. Make sure to use IP addresses not hostnames", p.second);
            g_outputBuffer += (fmt % "setSiblings(): Error parsing address/port" % p.second % "Make sure to use IP addresses not hostnames" % e.reason).str();
          }
	}
	g_siblings.setState(v);
      });
  }
  else {
    c_lua.writeFunction("setSiblings", [](const vector<pair<int, string>>& parts) { });
  }

  if (!allow_report) {
    c_lua.writeFunction("siblingListener", [](const std::string& address) {
	ComboAddress ca;
	try {
	  ca = ComboAddress(address, 4001);
	}
	catch (const WforceException& e) {
          boost::format fmt("%s (%s)\n");
          errlog("siblingListener() error parsing address/port [%s]. Make sure to use IP addresses not hostnames", address);
          g_outputBuffer += (fmt % "siblingListener(): Error parsing address/port. Make sure to use IP addresses not hostnames" % e.reason).str();
	  return;
	}

	auto launch = [ca]() {
	  thread t1(receiveReplicationOperations, ca);
	  t1.detach();
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

  if (!allow_report) {
    c_lua.writeFunction("setACL", [](const vector<pair<int, string>>& parts) {
	NetmaskGroup nmg;
	for(const auto& p : parts) {
	  nmg.addMask(p.second);
	}
	g_webserver.setACL(nmg);
      });
  }
  else {
    c_lua.writeFunction("setACL", [](const vector<pair<int, string>>& parts) { });
  }

  if (!allow_report) {
    c_lua.writeFunction("showACL", []() {
	vector<string> vec;

	g_webserver.getACL().toStringVector(&vec);

	for(const auto& s : vec)
	  g_outputBuffer+=s+"\n";

      });
  }
  else {
    c_lua.writeFunction("showACL", []() { });
  }

  c_lua.writeFunction("shutdown", []() { _exit(0);} );

  if (!allow_report) {
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

  if (!allow_report) {
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

  if (!allow_report) {
    c_lua.writeFunction("stats", []() {
	boost::format fmt("%d reports, %d allow-queries (%d denies)\n");
	g_outputBuffer = (fmt % g_stats.reports % g_stats.allows % g_stats.denieds).str();  
      });
  }
  else {
    c_lua.writeFunction("stats", []() { });
  }

  if (!allow_report) {
    c_lua.writeFunction("siblings", []() {
      auto siblings = g_siblings.getCopy();
      boost::format fmt("%-35s %-15d %-14d %-15d %-14d   %s\n");
      g_outputBuffer= (fmt % "Address" % "Send Successes" % "Send Failures" % "Rcv Successes" % "Rcv Failures" % "Note").str();
      for(const auto& s : siblings)
	g_outputBuffer += (fmt % s->rem.toStringWithPort() % s->success % s->failures % s->rcvd_success % s->rcvd_fail % (s->d_ignoreself ? "Self" : "") ).str();
      
    });
  } 
  else {
    c_lua.writeFunction("siblings", []() { });
  }	

  if (!allow_report) {
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

  if (!allow_report) {
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
  
  if (!allow_report) {
    c_lua.writeFunction("setNumLuaStates", [](int numStates) {
	g_num_luastates = numStates;
      });
  }
  else {
    c_lua.writeFunction("setNumLuaStates", [](int numStates) { });    
  }

  if (!allow_report) {
    c_lua.writeFunction("setNumWorkerThreads", [](int numThreads) {
	// the number of threads used to process allow/report commands
	g_webserver.setNumWorkerThreads(numThreads);
      });
  }
  else {
    c_lua.writeFunction("setNumWorkerThreads", [](int numThreads) { });    
  }

   if (!allow_report) {
    c_lua.writeFunction("setNumSiblingThreads", [](int numThreads) {
	// the number of threads used to process sibling reports
	g_num_sibling_threads = numThreads;
      });
  }
  else {
    c_lua.writeFunction("setNumSiblingThreads", [](int numThreads) { });    
  }

#ifdef HAVE_GEOIP
  if (!allow_report) {
      c_lua.writeFunction("initGeoIPDB", []() {
	  try {
	    g_wfgeodb.initGeoIPDB(WFGeoIPDBType::GEOIP_COUNTRY|WFGeoIPDBType::GEOIP_COUNTRY_V6);
	  }
	  catch (const WforceException& e) {
            boost::format fmt("%s (%s)\n");
            errlog("initGeoIPDB(): Error initialising GeoIP (%s)", e.reason);
            g_outputBuffer += (fmt % "initGeoIPDB(): Error loading GeoIP" % e.reason).str();
	  }
	});
  }
  else {
    c_lua.writeFunction("initGeoIPDB", []() { });
  }
  if (!allow_report) {
    c_lua.writeFunction("reloadGeoIPDBs", []() {
	try {
	  g_wfgeodb.reload();
	}
	catch (const WforceException& e) {
	  boost::format fmt("%s (%s)\n");
	  errlog("reloadGeoIPDBs(): Error reloading GeoIP (%s)", e.reason);
	  g_outputBuffer += (fmt % "reloadGeoIPDBs(): Error reloading GeoIP" % e.reason).str();
	}
	g_outputBuffer += "reloadGeoIPDBs() successful\n";
      });
  }
  else {
    c_lua.writeFunction("reloadGeoIPDBs", []() { });
  }
  c_lua.writeFunction("lookupCountry", [](ComboAddress address) {
      return g_wfgeodb.lookupCountry(address);
    });
  if (!allow_report) {
      c_lua.writeFunction("initGeoIPCityDB", []() {
	  try {
	    g_wfgeodb.initGeoIPDB(WFGeoIPDBType::GEOIP_CITY|WFGeoIPDBType::GEOIP_CITY_V6);
	  }
	  catch (const WforceException& e) {
            boost::format fmt("%s (%s)\n");
            errlog("initGeoIPCityDB(): Error initialising GeoIP (%s)", e.reason);
            g_outputBuffer += (fmt % "initGeoIPCityDB(): Error loading GeoIP" % e.reason).str();
	  }
	});

  }
  else {
    c_lua.writeFunction("initGeoIPCityDB", []() { });
  }
  c_lua.writeFunction("lookupCity", [](ComboAddress address) {
      return g_wfgeodb.lookupCity(address);
    });
  c_lua.registerMember("country_code", &WFGeoIPRecord::country_code);
  c_lua.registerMember("country_name", &WFGeoIPRecord::country_name);
  c_lua.registerMember("region", &WFGeoIPRecord::region);
  c_lua.registerMember("city", &WFGeoIPRecord::city);
  c_lua.registerMember("postal_code", &WFGeoIPRecord::postal_code);
  c_lua.registerMember("continent_code", &WFGeoIPRecord::continent_code);
  c_lua.registerMember("latitude", &WFGeoIPRecord::latitude);
  c_lua.registerMember("longitude", &WFGeoIPRecord::longitude);
  if (!allow_report) {
    c_lua.writeFunction("initGeoIPISPDB", []() {
	try {
	  g_wfgeodb.initGeoIPDB(WFGeoIPDBType::GEOIP_ISP|WFGeoIPDBType::GEOIP_ISP_V6);
	}
	catch (const WforceException& e) {
          boost::format fmt("%s (%s)\n");
          errlog("initGeoIPISPDB(): Error initialising GeoIP (%s)", e.reason);
          g_outputBuffer += (fmt % "initGeoIPISPDB(): Error loading GeoIP" % e.reason).str();
	}
      });
  }
  else {
    c_lua.writeFunction("initGeoIPISPDB", []() { });
  }
  c_lua.writeFunction("lookupISP", [](ComboAddress address) {
      return g_wfgeodb.lookupISP(address);
    });
#endif // HAVE_GEOIP

#ifdef HAVE_GETDNS
  if (!allow_report) {
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
  if (!allow_report) {
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
  
  if (!allow_report) {
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
  
  if (!allow_report) {
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

  if (!allow_report) {
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

  if (!allow_report) {
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
  c_lua.registerFunction("twEnableReplication", &TWStringStatsDBWrapper::enableReplication);
  c_lua.registerFunction("twGetName", &TWStringStatsDBWrapper::getDBName);

  c_lua.writeFunction("debugLog", [](const std::string& msg, const std::vector<pair<std::string, std::string>>& kvs) {
      if (g_verbose) {
	std::ostringstream os;
	os << msg << ": ";
	for (const auto& i : kvs) {
	  os << i.first << "="<< "\"" << i.second << "\"" << " ";
	}
	debuglog(os.str().c_str());
      }
    });
    
  c_lua.writeFunction("vinfoLog", [](const std::string& msg, const std::vector<pair<std::string, std::string>>& kvs) {
      if (g_verbose) {
	std::ostringstream os;
	os << msg << ": ";
	for (const auto& i : kvs) {
	  os << i.first << "="<< "\"" << i.second << "\"" << " ";
	}
	infolog(os.str().c_str());
      }
    });

  c_lua.writeFunction("infoLog", [](const std::string& msg, const std::vector<pair<std::string, std::string>>& kvs) {
      std::ostringstream os;
      os << msg << ": ";
      for (const auto& i : kvs) {
	os << i.first << "="<< "\"" << i.second << "\"" << " ";
      }
      infolog(os.str().c_str());
    });

  c_lua.writeFunction("warnLog", [](const std::string& msg, const std::vector<pair<std::string, std::string>>& kvs) {
      std::ostringstream os;
      os << msg << ": ";
      for (const auto& i : kvs) {
	os << i.first << "="<< "\"" << i.second << "\"" << " ";
      }
      warnlog(os.str().c_str());
    });

  c_lua.writeFunction("errorLog", [](const std::string& msg, const std::vector<pair<std::string, std::string>>& kvs) {
      std::ostringstream os;
      os << msg << ": ";
      for (const auto& i : kvs) {
	os << i.first << "="<< "\"" << i.second << "\"" << " ";
      }	
      errlog(os.str().c_str());
    });

  if (!allow_report) {
    c_lua.writeFunction("setVerboseAllowLog()", []() {
	g_allowlog_verbose = true;
      });
  }
  else {
    c_lua.writeFunction("setVerboseAllowLog()", []() { });
  }

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

  if (!allow_report) {
    c_lua.writeFunction("blacklistPersistDB", [](const std::string& ip, unsigned int port) {
	g_bl_db.makePersistent(ip, port);
      });
    c_lua.writeFunction("blacklistPersistReplicated", []() { g_bl_db.persistReplicated(); });
    c_lua.writeFunction("blacklistPersistConnectTimeout", [](int timeout_secs) { g_bl_db.setConnectTimeout(timeout_secs); });
  }
  else {
    c_lua.writeFunction("blacklistPersistDB", [](const std::string& ip, unsigned int port) {});
    c_lua.writeFunction("blacklistPersistReplicated", []() {});
    c_lua.writeFunction("blacklistPersistConnectTimeout", [](int timeout_secs) {});
  }
  
  c_lua.registerMember("t", &LoginTuple::t);
  c_lua.registerMember("remote", &LoginTuple::remote);
  c_lua.registerMember("login", &LoginTuple::login);
  c_lua.registerMember("pwhash", &LoginTuple::pwhash);
  c_lua.registerMember("success", &LoginTuple::success);
  c_lua.registerMember("policy_reject", &LoginTuple::policy_reject);
  c_lua.registerMember("attrs", &LoginTuple::attrs);
  c_lua.registerMember("attrs_mv", &LoginTuple::attrs_mv);
  c_lua.registerMember("protocol", &LoginTuple::protocol);
  c_lua.registerMember("tls", &LoginTuple::tls);
  c_lua.registerMember("device_id", &LoginTuple::device_id);
  c_lua.registerMember("device_attrs", &LoginTuple::device_attrs);

  c_lua.registerFunction<string(ComboAddress::*)()>("tostring", [](const ComboAddress& ca) { return ca.toString(); });

  c_lua.writeFunction("newCA", [](string address) {
      try {
	return ComboAddress(address);
      }
      catch (const WforceException& e) {
        boost::format fmt("%s (%s)\n");
        g_outputBuffer += (fmt % "newCA(): error parsing address/port. Make sure to use IP addresses not hostnames" % e.reason).str();
        errlog("newCA() error parsing address/port [%s]. Make sure to use IP addresses not hostnames", address);
        return ComboAddress();
      }
    } );

  c_lua.writeFunction("newNetmask", [](string address) {
      try {
	return Netmask(address);
      }
      catch (const WforceException& e) {
        boost::format fmt("%s (%s)\n");
        g_outputBuffer += (fmt % "newNetmask(): error parsing address/port. Make sure to use IP addresses not hostnames" % e.reason).str();
        errlog("newNetmask() error parsing netmask [%s]. Use x.x.x.x/y notation.", address);
        return Netmask();
      }
    } );
  
  c_lua.writeFunction("newNetmaskGroup", []() { return NetmaskGroup(); } );

  c_lua.registerFunction<void(NetmaskGroup::*)(const std::string&)>("addMask", [](NetmaskGroup& nmg, const std::string& mask) {
      nmg.addMask(mask);
    });
  c_lua.registerFunction("match", (bool (NetmaskGroup::*)(const ComboAddress&) const)&NetmaskGroup::match);
  g_lua.registerFunction("size", &NetmaskGroup::size);
  g_lua.registerFunction("clear", &NetmaskGroup::clear);

  if (allow_report) {
    c_lua.writeFunction("setAllow", [&allow_func](allow_t func) { allow_func=func;});
  }
  else {
    c_lua.writeFunction("setAllow", [](allow_t func) { });    
  }

  if (allow_report) {
    c_lua.writeFunction("setReport", [&report_func](report_t func) { report_func=func;});
  }
  else {
    c_lua.writeFunction("setReport", [](report_t func) { });
  }

  if (allow_report) {
    c_lua.writeFunction("setReset", [&reset_func](reset_t func) { reset_func=func;});
  }
  else {
    c_lua.writeFunction("setReset", [](reset_t func) { });
  }

  if (allow_report) {
    c_lua.writeFunction("setCanonicalize", [&canon_func](canonicalize_t func) { canon_func=func;});
  }
  else {
    c_lua.writeFunction("setCanonicalize", [](canonicalize_t func) { });
  }

  c_lua.registerMember("attrs", &CustomFuncArgs::attrs);
  c_lua.registerMember("attrs_mv", &CustomFuncArgs::attrs_mv);

  c_lua.writeFunction("setCustomEndpoint", [&custom_func_map, allow_report, client](const std::string& f_name, bool reportSink, custom_func_t func) {
      CustomFuncMapObject cobj;
      cobj.c_func = func;
      cobj.c_reportSink = reportSink;
      custom_func_map.insert(std::make_pair(f_name, cobj));
      if (!allow_report && !client) {
	// register a webserver command
	g_webserver.registerFunc(f_name, HTTPVerb::POST, parseCustomCmd);
	noticelog("Registering custom endpoint [%s]", f_name);
      }
    });

  if (!allow_report) {
    c_lua.writeFunction("showCustomEndpoints", []() {
	boost::format fmt("%-30.30s %-s \n");
	g_outputBuffer = (fmt % "Custom Endpoint" % "Send to Report Sink?").str();
	for (const auto& i : g_custom_func_map) {
	  std::string reportStr = i.second.c_reportSink == true ? "true" : "false";
	  g_outputBuffer += (fmt % i.first % reportStr).str();
	}
      });
  }
  else {
    c_lua.writeFunction("showCustomEndpoints", []() { });
  }

  if (!allow_report) {
    c_lua.writeFunction("showPerfStats", []() {
	g_outputBuffer += getPerfStatsString();
      });
  }
  else {
    c_lua.writeFunction("showPerfStats", []() {
      });
  }
  
  if (!allow_report) {
    c_lua.writeFunction("setNumWebHookThreads", [](unsigned int num_threads) { g_webhook_runner.setNumThreads(num_threads); });
  }
  else {
    c_lua.writeFunction("setNumWebHookThreads", [](unsigned int num_threads) { });
  }

  if (!allow_report) {
    c_lua.writeFunction("setWebHookQueueSize", [](unsigned int queue_size) { g_webhook_runner.setMaxQueueSize(queue_size); });
  }
  else {
    c_lua.writeFunction("setWebHookQueueSize", [](unsigned int queue_size) { });
  }

  if (!allow_report) {
    c_lua.writeFunction("setNumWebHookConnsPerThread", [](unsigned int num_conns) { g_webhook_runner.setMaxConns(num_conns); });
  }
  else {
    c_lua.writeFunction("setNumWebHookConnsPerThread", [](unsigned int num_conns) { });
  }

  
  if (!allow_report) {
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

  if (!allow_report) {
    c_lua.writeFunction("showCustomWebHooks", []() {
	auto webhooks = g_custom_webhook_db.getWebHooks();
	boost::format fmt("%-9d %-20.20s %-9d %-9d %-s\n");
	g_outputBuffer= (fmt % "ID" % "Name" % "Successes" % "Failures" % "URL").str();
	for(const auto& i : webhooks) {
	  if (auto is = i.lock())
	    if (is)
	      g_outputBuffer += (fmt % is->getID() % is->getName() % is->getSuccess() % is->getFailed() % is->getConfigKey("url")).str();
	}
      });
  }
  else {
    c_lua.writeFunction("showCustomWebHooks", []() { });
  }

  if (allow_report) {
    c_lua.writeFunction("runCustomWebHook", [](const std::string& wh_name, const std::string& wh_data) {
	auto whwp = g_custom_webhook_db.getWebHook(wh_name);
	if (auto whp = whwp.lock()) {
	  if (whp) {
	    g_webhook_runner.runHook(wh_name, whp, wh_data);
	  }
	  else {
	    errlog("Attempting to run custom webhook with name %d but no such webhook exists!", wh_name); 
	  }
	} else {
	  errlog("Attempting to run custom webhook with name %d but no such webhook exists!", wh_name); 	  
	}
      });
  }
  else {
    c_lua.writeFunction("runCustomWebHook", []() { });
  }
  
  if (!(allow_report || client)) {
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

  if (!(allow_report || client)) {
    c_lua.writeFunction("addCustomWebHook", [](const std::string& name,
					       const std::vector<std::pair<std::string, std::string>>& ck_vec) {
	  std::string err;
	  WHConfigMap config_keys;

	  auto id = g_custom_webhook_db.getNewID();
	  for (const auto& ck : ck_vec) {
	    config_keys.insert(ck);
	  }
	  auto ret = g_custom_webhook_db.addWebHook(CustomWebHook(id, name, true, config_keys), err);
	  if (ret != true) {
	    errlog("Registering custom webhook id=%d name=%d from Lua failed [%s]", id, name, err);
	  }
	});
  }
  else {
    c_lua.writeFunction("addCustomWebHook", [](const std::string& name, const std::vector<std::pair<std::string, std::string>>& ck_vec) { });
  }
  
  if (!allow_report) {
    c_lua.writeFunction("makeKey", []() {
	g_outputBuffer="setKey("+newKey()+")\n";
      });
  }
  else {
    c_lua.writeFunction("makeKey", []() { });    
  }

  if (!allow_report) {
    c_lua.writeFunction("setKey", [](const std::string& key) {
	string newkey;
	if(B64Decode(key, newkey) < 0) {
	  g_outputBuffer=string("Unable to decode ")+key+" as Base64";
	  errlog("%s", g_outputBuffer);
	}
	else
	  g_key = newkey;
      });
  }
  else {
    c_lua.writeFunction("setKey", [](const std::string& key) { });
  }

  if (!allow_report) {
    c_lua.writeFunction("showVersion", []() {
	g_outputBuffer = "wforce " + std::string(VERSION) + "\n";
      });
  }
  else {
    c_lua.writeFunction("showVersion", []() { });
  }
  
  if (!allow_report) {
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
  
  std::ifstream ifs(config);
  if(!ifs) 
    warnlog("Unable to read configuration from '%s'", config);
  else if (!allow_report)
    infolog("Read configuration from '%s'", config);

  c_lua.executeCode(ifs);
  if (!allow_report) {
    auto ret=*g_launchWork;
    delete g_launchWork;
    g_launchWork=0;
    return ret;
  }
  else {
    return vector<std::function<void(void)>>();
  }
}
