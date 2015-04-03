#include "wforce.hh"
#include <thread>
#include "dolog.hh"
#include "sodcrypto.hh"
#include "base64.hh"
#include <fstream>

using std::thread;

static vector<std::function<void(void)>>* g_launchWork;

vector<std::function<void(void)>> setupLua(bool client, const std::string& config)
{
  g_launchWork= new vector<std::function<void(void)>>();
  g_lua.writeFunction("addACL", [](const std::string& domain) {
      g_ACL.modify([domain](NetmaskGroup& nmg) { nmg.addMask(domain); });
    });

  g_lua.writeFunction("addSibling", [](const std::string& address) {
      ComboAddress ca(address, 4001);
      g_siblings.modify([ca](vector<shared_ptr<Sibling>>& v) { v.push_back(std::make_shared<Sibling>(ca)); });
    });

  g_lua.writeFunction("setSiblings", [](const vector<pair<int, string>>& parts) {
      vector<shared_ptr<Sibling>> v;
      for(const auto& p : parts) {
	v.push_back(std::make_shared<Sibling>(ComboAddress(p.second, 4001)));
      }
      g_siblings.setState(v);
  });


  g_lua.writeFunction("siblingListener", [](const std::string& address) {
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

  g_lua.writeFunction("addLocal", [client](const std::string& addr) {
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
  g_lua.writeFunction("setACL", [](const vector<pair<int, string>>& parts) {
      NetmaskGroup nmg;
      for(const auto& p : parts) {
	nmg.addMask(p.second);
      }
      g_ACL.setState(nmg);
  });
  g_lua.writeFunction("showACL", []() {
      vector<string> vec;

      g_ACL.getCopy().toStringVector(&vec);

      for(const auto& s : vec)
        g_outputBuffer+=s+"\n";

    });
  g_lua.writeFunction("shutdown", []() { _exit(0);} );



  g_lua.writeFunction("webserver", [client](const std::string& address, const std::string& password) {
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
  g_lua.writeFunction("controlSocket", [client](const std::string& str) {
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


  g_lua.writeFunction("report", [](string remote, string login, string pwhash, bool success) {
      LoginTuple lt;
      lt.t=time(0);
      lt.remote=ComboAddress(remote);
      lt.login=login;
      lt.pwhash=pwhash;
      lt.success=success;
      g_wfdb.reportTuple(lt);
    });

  g_lua.writeFunction("allow", [](string remote, string login, string pwhash) {
      LoginTuple lt;
      lt.remote=ComboAddress(remote);
      lt.login=login;
      lt.pwhash=pwhash;
      lt.success=0;
      return g_allow(&g_wfdb, lt); // no locking needed, we are in Lua here already!
    });

  
  g_lua.writeFunction("countFailures", [](ComboAddress remote, int seconds) {
      return g_wfdb.countFailures(remote, seconds);
    });
  g_lua.writeFunction("countDiffFailures", [](ComboAddress remote, int seconds) {
      return g_wfdb.countDiffFailures(remote, seconds);
    });

  g_lua.registerMember("t", &LoginTuple::t);
  g_lua.registerMember("remote", &LoginTuple::remote);
  g_lua.registerMember("login", &LoginTuple::login);
  g_lua.registerMember("pwhash", &LoginTuple::pwhash);
  g_lua.registerMember("success", &LoginTuple::success);
  g_lua.writeVariable("wfdb", &g_wfdb);
  g_lua.registerFunction("report", &WForceDB::reportTuple);
  g_lua.registerFunction("getTuples", &WForceDB::getTuples);

  g_lua.registerFunction("tostring", &ComboAddress::toString);
  g_lua.writeFunction("newCA", [](string address) { return ComboAddress(address); } );
  g_lua.registerFunction("countDiffFailuresAddress", static_cast<int (WForceDB::*)(const ComboAddress&, int) const>(&WForceDB::countDiffFailures));
  g_lua.registerFunction("countDiffFailuresAddressLogin", static_cast<int (WForceDB::*)(const ComboAddress&, string, int) const>(&WForceDB::countDiffFailures));

  g_lua.writeFunction("newNetmaskGroup", []() { return NetmaskGroup(); } );

  g_lua.registerFunction("addMask", &NetmaskGroup::addMask);
  g_lua.registerFunction("match", static_cast<bool(NetmaskGroup::*)(const ComboAddress&) const>(&NetmaskGroup::match));

  g_lua.writeFunction("setAllow", [](allow_t func) { g_allow=func;});
  g_lua.writeFunction("testCrypto", [](string testmsg)
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

  
  std::ifstream ifs(config);
  if(!ifs) 
    warnlog("Unable to read configuration from '%s'", config);
  else
    infolog("Read configuration from '%s'", config);

  g_lua.executeCode(ifs);
  auto ret=*g_launchWork;
  delete g_launchWork;
  g_launchWork=0;
  return ret;
}
