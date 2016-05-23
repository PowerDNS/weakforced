#include "config.h"
#include "wforce.hh"
#include <thread>
#include "dolog.hh"
#include "sodcrypto.hh"
#include "base64.hh"
#include "twmap.hh"
#include "blacklist.hh"
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
					   std::function<AllowReturn(const LoginTuple&)>& allow_func, 
					   std::function<void(const LoginTuple&)>& report_func,
					   std::function<bool(const std::string&, const std::string&, const ComboAddress&)>& reset_func,
					   const std::string& config)
{
  g_launchWork= new vector<std::function<void(void)>>();
  
  if (!allow_report) {
    c_lua.writeFunction("addACL", [](const std::string& domain) {
	g_ACL.modify([domain](NetmaskGroup& nmg) { nmg.addMask(domain); });
      });
  }
  else { // empty function for allow/report - this stops parsing errors or weirdness
    c_lua.writeFunction("addACL", [](const std::string& domain) { });
  }

  if (!allow_report) {
    c_lua.writeFunction("addSibling", [](const std::string& address) {
	ComboAddress ca(address, 4001);
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
	  v.push_back(std::make_shared<Sibling>(ComboAddress(p.second, 4001)));
	}
	g_siblings.setState(v);
      });
  }
  else {
    c_lua.writeFunction("setSiblings", [](const vector<pair<int, string>>& parts) { });
  }
    
  if (!allow_report) {
    c_lua.writeFunction("siblingListener", [](const std::string& address) {
	ComboAddress ca(address, 4001);
      
	auto launch = [ca]() {
	  thread t1(receiveReports, ca);
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
    c_lua.writeFunction("addLocal", [client](const std::string& addr) {
	if(client)
	  return;
	try {
	  ComboAddress loc(addr, 53);
	  g_locals.push_back(loc); /// only works pre-startup, so no sync necessary
	}
	catch(std::exception& e) {
	  g_outputBuffer="Error: "+string(e.what())+"\n";
	}
      });
  }
  else {
    c_lua.writeFunction("addLocal", [client](const std::string& addr) { });
  }

  if (!allow_report) {
    c_lua.writeFunction("setACL", [](const vector<pair<int, string>>& parts) {
	NetmaskGroup nmg;
	for(const auto& p : parts) {
	  nmg.addMask(p.second);
	}
	g_ACL.setState(nmg);
      });
  }
  else {
    c_lua.writeFunction("setACL", [](const vector<pair<int, string>>& parts) { });
  }

  if (!allow_report) {
    c_lua.writeFunction("showACL", []() {
	vector<string> vec;

	g_ACL.getCopy().toStringVector(&vec);

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
	ComboAddress local(address);
	try {
	  int sock = socket(local.sin4.sin_family, SOCK_STREAM, 0);
	  SSetsockopt(sock, SOL_SOCKET, SO_REUSEADDR, 1);
	  SBind(sock, local);
	  SListen(sock, 5);
	  auto launch=[sock, local, password]() {
	    thread t(dnsdistWebserverThread, sock, local, password);
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
    c_lua.writeFunction("webserver", [client](const std::string& address, const std::string& password) { });
  }

  if (!allow_report) {
    c_lua.writeFunction("controlSocket", [client](const std::string& str) {
	ComboAddress local(str, 5199);

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
    c_lua.writeFunction("controlSocket", [client](const std::string& str) { });
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
      boost::format fmt("%-35s %-9d %-9d    %s\n");
      g_outputBuffer= (fmt % "Address" % "Sucesses" % "Failures" % "Note").str();
      for(const auto& s : siblings)
	g_outputBuffer += (fmt % s->rem.toStringWithPort() % s->success % s->failures % (s->d_ignoreself ? "Self" : "") ).str();
      
    });
  } 
  else {
    c_lua.writeFunction("siblings", []() { });
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
	g_num_worker_threads = numThreads;
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
	g_wfgeodb.initGeoIPDB();
      });
  }
  else {
    c_lua.writeFunction("initGeoIPDB", []() { });
  }
  c_lua.writeFunction("lookupCountry", [](ComboAddress address) {
      return g_wfgeodb.lookupCountry(address);
    });
#endif // HAVE_GEOIP

#ifdef HAVE_GETDNS
  if (!allow_report) {
    c_lua.writeFunction("newDNSResolver", [](const std::string& name) { 
	auto resolvp = std::make_shared<WFResolver>(); 
	resolvMap.insert(std::pair<std::string, std::shared_ptr<WFResolver>>(name, resolvp));
      });
  }
  else {
    c_lua.writeFunction("newDNSResolver", [](const std::string& name) { });
  }
  c_lua.registerFunction("addResolver", &WFResolver::add_resolver);
  c_lua.registerFunction("setRequestTimeout", &WFResolver::set_request_timeout);
  c_lua.registerFunction("setNumContexts", &WFResolver::set_num_contexts);
  c_lua.writeFunction("getDNSResolver", [](const std::string& name) {
      auto it = resolvMap.find(name);
      if (it != resolvMap.end())
	return *(it->second); // copy
      else
	return WFResolver(); // copy
  });
  c_lua.registerFunction("lookupAddrByName", &WFResolver::lookup_address_by_name);
  c_lua.registerFunction("lookupNameByAddr", &WFResolver::lookup_name_by_address);
  c_lua.registerFunction("lookupRBL", &WFResolver::lookupRBL);
  // The following "show.." functions are mainly for regression tests
  if (!allow_report) {
    c_lua.writeFunction("showAddrByName", [](WFResolver resolv, string name) {
	std::vector<std::string> retvec = resolv.lookup_address_by_name(name, 1);
	boost::format fmt("%s %s\n");
	for (const auto s : retvec) {
	  g_outputBuffer += (fmt % name % s).str();
	}
      });
  } else {
    c_lua.writeFunction("showAddrByName", [](WFResolver resolv, string name) { });
  }
  
  if (!allow_report) {
    c_lua.writeFunction("showNameByAddr", [](WFResolver resolv, ComboAddress address) {
	std::vector<std::string> retvec = resolv.lookup_name_by_address(address, 1);
	boost::format fmt("%s %s\n");
	for (const auto s : retvec) {
	  g_outputBuffer += (fmt % address.toString() % s).str();
	}
      });
  }
  else {
    c_lua.writeFunction("showNameByAddr", [](WFResolver resolv, ComboAddress address) { });
  }
  
  if (!allow_report) {
    c_lua.writeFunction("showRBL", [](WFResolver resolv, ComboAddress address, string rblname) {
	std::vector<std::string> retvec = resolv.lookupRBL(address, rblname, 1);
	boost::format fmt("%s\n");
	for (const auto s : retvec) {
	  g_outputBuffer += (fmt % s).str();
	}	
      });	
  }
  else {
    c_lua.writeFunction("showRBL", [](WFResolver resolv, ComboAddress address, string rblname) { });    
  }
#endif // HAVE_GETDNS

  if (!allow_report) {
    c_lua.writeFunction("newStringStatsDB", [](const std::string& name, int window_size, int num_windows, const std::vector<pair<std::string, std::string>>& fmvec) {
	auto twsdbw = TWStringStatsDBWrapper(name, window_size, num_windows, fmvec);
	// register this statsDB in a map for retrieval later
	std::lock_guard<std::mutex> lock(dbMap_mutx);
	dbMap.insert(std::pair<std::string, TWStringStatsDBWrapper>(name, twsdbw));
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
      else
	return TWStringStatsDBWrapper("none", 1, 1);
    });

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

  c_lua.writeFunction("infoLog", [](const std::string& msg, const std::vector<pair<std::string, std::string>>& kvs) {
      std::ostringstream os;
      os << msg << ": ";
      for (const auto& i : kvs) {
	os << i.first << "="<< "\"" << i.second << "\"" << " ";
      }
      infolog(os.str().c_str());
    });

  c_lua.writeFunction("blacklistIP", [](const ComboAddress& ca, unsigned int seconds, const std::string& reason) {
      bl_db.addEntry(ca, seconds, reason);
    });

  c_lua.writeFunction("blacklistLogin", [](const std::string& login, unsigned int seconds, const std::string& reason) {
      bl_db.addEntry(login, seconds, reason);
    });

  c_lua.writeFunction("blacklistIPLogin", [](const ComboAddress& ca, const std::string& login, unsigned int seconds, const std::string& reason) {
      bl_db.addEntry(ca, login, seconds, reason);
    });

  c_lua.registerMember("t", &LoginTuple::t);
  c_lua.registerMember("remote", &LoginTuple::remote);
  c_lua.registerMember("login", &LoginTuple::login);
  c_lua.registerMember("pwhash", &LoginTuple::pwhash);
  c_lua.registerMember("success", &LoginTuple::success);
  c_lua.registerMember("attrs", &LoginTuple::attrs);
  c_lua.registerMember("attrs_mv", &LoginTuple::attrs_mv);

  c_lua.registerFunction("tostring", &ComboAddress::toString);
  c_lua.writeFunction("newCA", [](string address) { return ComboAddress(address); } );

  c_lua.writeFunction("newNetmaskGroup", []() { return NetmaskGroup(); } );

  c_lua.registerFunction("addMask", &NetmaskGroup::addMask);
  c_lua.registerFunction("match", static_cast<bool(NetmaskGroup::*)(const ComboAddress&) const>(&NetmaskGroup::match));

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
	if(B64Decode(key, g_key) < 0) {
	  g_outputBuffer=string("Unable to decode ")+key+" as Base64";
	  errlog("%s", g_outputBuffer);
	}
      });
  }
  else {
    c_lua.writeFunction("setKey", [](const std::string& key) { });
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
  else
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
