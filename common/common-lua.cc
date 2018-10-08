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

#include "common-lua.hh"

#ifdef HAVE_GEOIP
#include "wforce-geoip.hh"
#endif
#ifdef HAVE_MMDB
#include "wforce-geoip2.hh"
#endif

using std::thread;

void setupCommonLua(bool client,
                    bool multi_lua,
                    LuaContext& c_lua,
                    const std::string& config)
{
  if (!multi_lua) {
    c_lua.writeFunction("addACL", [](const std::string& domain) {
	g_webserver.addACL(domain);
      });
  }
  else { // empty function for allow/report - this stops parsing errors or weirdness
    c_lua.writeFunction("addACL", [](const std::string& domain) { });
  }

  if (!multi_lua) {
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

  if (!multi_lua) {
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

  if (!multi_lua) {
    c_lua.writeFunction("setNumLuaStates", [](int numStates) {
	g_num_luastates = numStates;
      });
  }
  else {
    c_lua.writeFunction("setNumLuaStates", [](int numStates) { });    
  }

  if (!multi_lua) {
    c_lua.writeFunction("setNumWorkerThreads", [](int numThreads) {
	// the number of threads used to process webserver commands
	g_webserver.setNumWorkerThreads(numThreads);
      });
  }
  else {
    c_lua.writeFunction("setNumWorkerThreads", [](int numThreads) { });    
  }

  if (!multi_lua) {
    c_lua.writeFunction("setMaxWebserverConns", [](int max_conns) {
	// the max number of active webserver connections
	g_webserver.setMaxConns(max_conns);
      });
  }
  else {
    c_lua.writeFunction("setMaxWebserverConns", [](int max_conns) { });
  }
  
  c_lua.writeFunction("debugLog", [](const std::string& msg, const std::vector<pair<std::string, std::string>>& kvs) {
      if (g_verbose) {
	std::ostringstream os;
	os << msg;
        if (kvs.size())
          os << ": ";
	for (const auto& i : kvs) {
	  os << i.first << "="<< "\"" << i.second << "\"" << " ";
	}
	debuglog(os.str().c_str());
      }
    });
    
  c_lua.writeFunction("vinfoLog", [](const std::string& msg, const std::vector<pair<std::string, std::string>>& kvs) {
      if (g_verbose) {
	std::ostringstream os;
	os << msg;
        if (kvs.size())
          os << ": ";
	for (const auto& i : kvs) {
	  os << i.first << "="<< "\"" << i.second << "\"" << " ";
	}
	infolog(os.str().c_str());
      }
    });
  
  c_lua.writeFunction("infoLog", [](const std::string& msg, const std::vector<pair<std::string, std::string>>& kvs) {
      std::ostringstream os;
      os << msg;
      if (kvs.size())
        os << ": ";
      for (const auto& i : kvs) {
	os << i.first << "="<< "\"" << i.second << "\"" << " ";
      }
      infolog(os.str().c_str());
    });

  c_lua.writeFunction("warnLog", [](const std::string& msg, const std::vector<pair<std::string, std::string>>& kvs) {
      std::ostringstream os;
      os << msg;
      if (kvs.size())
        os << ": ";
      for (const auto& i : kvs) {
	os << i.first << "="<< "\"" << i.second << "\"" << " ";
      }
      warnlog(os.str().c_str());
    });

  c_lua.writeFunction("errorLog", [](const std::string& msg, const std::vector<pair<std::string, std::string>>& kvs) {
      std::ostringstream os;
      os << msg;
      if (kvs.size())
        os << ": ";
      for (const auto& i : kvs) {
	os << i.first << "="<< "\"" << i.second << "\"" << " ";
      }	
      errlog(os.str().c_str());
    });

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

  if (!multi_lua) {
    c_lua.writeFunction("addCustomStat", [](const std::string& stat_name) { addCustomStat(stat_name); });
  }
  else {
    c_lua.writeFunction("addCustomStat", [](const std::string& stat_name) {} );
  }

  if (multi_lua) {
    c_lua.writeFunction("incCustomStat", [](const std::string& stat_name) { incCustomStat(stat_name); });
  }
  else {
    c_lua.writeFunction("incCustomStat", [](const std::string& stat_name) {} );
  }
  
  if (!multi_lua) {
    c_lua.writeFunction("showPerfStats", []() {
	g_outputBuffer += getPerfStatsString();
      });
  }
  else {
    c_lua.writeFunction("showPerfStats", []() {
      });
  }

  if (!multi_lua) {
    c_lua.writeFunction("showCommandStats", []() {
	g_outputBuffer += getCommandStatsString();
      });
  }
  else {
    c_lua.writeFunction("showCommandStats", []() {
      });
  }

  if (!multi_lua) {
    c_lua.writeFunction("showCustomStats", []() {
	g_outputBuffer += getCustomStatsString();
      });
  }
  else {
    c_lua.writeFunction("showCustomStats", []() {
      });
  }
  
  if (!multi_lua) {
    c_lua.writeFunction("setNumWebHookThreads", [](unsigned int num_threads) { g_webhook_runner.setNumThreads(num_threads); });
  }
  else {
    c_lua.writeFunction("setNumWebHookThreads", [](unsigned int num_threads) { });
  }

  if (!multi_lua) {
    c_lua.writeFunction("setWebHookQueueSize", [](unsigned int queue_size) { g_webhook_runner.setMaxQueueSize(queue_size); });
  }
  else {
    c_lua.writeFunction("setWebHookQueueSize", [](unsigned int queue_size) { });
  }

  if (!multi_lua) {
    c_lua.writeFunction("setNumWebHookConnsPerThread", [](unsigned int num_conns) { g_webhook_runner.setMaxConns(num_conns); });
  }
  else {
    c_lua.writeFunction("setNumWebHookConnsPerThread", [](unsigned int num_conns) { });
  }

  if (!multi_lua) {
    c_lua.writeFunction("setWebHookTimeoutSecs", [](uint64_t timeout_secs) { g_webhook_runner.setTimeout(timeout_secs); });
  }
  else {
    c_lua.writeFunction("setWebHookTimeoutSecs", [](uint64_t timeout_secs) { });
  }
  
  if (!multi_lua) {
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

  if (multi_lua) {
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
  
  if (!(multi_lua || client)) {
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

  if (!multi_lua) {
    c_lua.writeFunction("makeKey", []() {
	g_outputBuffer="setKey("+newKey()+")\n";
      });
  }
  else {
    c_lua.writeFunction("makeKey", []() { });    
  }

  if (!multi_lua) {
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

#ifdef HAVE_GEOIP
  if (!multi_lua) {
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
  if (!multi_lua) {
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
  if (!multi_lua) {
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
  if (!multi_lua) {
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

#ifdef HAVE_MMDB

  if (!multi_lua) {
    c_lua.writeFunction("newGeoIP2DB", [](const std::string& name, const std::string& filename) {
        try {
          std::lock_guard<std::mutex> lock(geoip2_mutx);
          geoip2Map.emplace(std::make_pair(name, WFGeoIP2DB::makeWFGeoIP2DB(filename)));
        }
        catch (const WforceException& e) {
          boost::format fmt("%s (%s)\n");
          errlog("newGeoIPDB(): Error initialising GeoIP2DB (%s)", e.reason);
          g_outputBuffer += (fmt % "newGeoIP2DB(): Error loading GeoIP2 DB" % e.reason).str();
        }
      });
  }
  else {
    c_lua.writeFunction("newGeoIP2DB", [](const std::string& name, const std::string& filename) { });
  }

  c_lua.writeFunction("getGeoIP2DB", [](const std::string& name) {
      std::lock_guard<std::mutex> lock(geoip2_mutx);
      auto it = geoip2Map.find(name);
      if (it != geoip2Map.end())
	return it->second;
      else {
        errlog("getGeoIP2DB(): Could not find database named %s", name);
	return std::make_shared<WFGeoIP2DB>();
      }
    });
  
  c_lua.registerFunction("lookupCountry", &WFGeoIP2DB::lookupCountry);
  c_lua.registerFunction("lookupISP", &WFGeoIP2DB::lookupISP);
  c_lua.registerFunction("lookupCity", &WFGeoIP2DB::lookupCity);
#endif // HAVE_MMDB  
#endif // HAVE_GEOIP  
}
