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

#include "config.h"
#include "wforce.hh"
#include "sstuff.hh"
#include "misc.hh"
#include <netinet/tcp.h>
#include <limits>
#include "dolog.hh"
#include <readline/readline.h>
#include <readline/history.h>
#include "base64.hh"
#include <fstream>
#include "ext/json11/json11.hpp"
#include <unistd.h>
#include <sys/stat.h>
#include "sodcrypto.hh"
#include "blacklist.hh"
#include "perf-stats.hh"
#include "webhook.hh"

#include <getopt.h>
#ifdef HAVE_LIBSYSTEMD
#include <systemd/sd-daemon.h>
#endif
#include "ext/ctpl.h"

using std::atomic;
using std::thread;
bool g_verbose=false;

struct WForceStats g_stats;
bool g_console;

GlobalStateHolder<NetmaskGroup> g_ACL;
string g_outputBuffer;

WebHookRunner g_webhook_runner;
WebHookDB g_webhook_db;
WebHookDB g_custom_webhook_db;

bool getMsgLen(int fd, uint16_t* len)
try
{
  uint16_t raw;
  int ret = readn2(fd, &raw, 2);
  if(ret != 2)
    return false;
  *len = ntohs(raw);
  return true;
}
catch(...) {
   return false;
}

bool putMsgLen(int fd, uint16_t len)
try
{
  uint16_t raw = htons(len);
  int ret = writen2(fd, &raw, 2);
  return ret==2;
}
catch(...) {
  return false;
}

std::mutex g_luamutex;
LuaContext g_lua;
int g_num_luastates=NUM_LUA_STATES;
std::shared_ptr<LuaMultiThread> g_luamultip;

static void daemonize(void)
{
  if(fork())
    _exit(0); // bye bye
  
  setsid(); 

  int i=open("/dev/null",O_RDWR); /* open stdin */
  if(i < 0) 
    ; // L<<Logger::Critical<<"Unable to open /dev/null: "<<stringerror()<<endl;
  else {
    dup2(i,0); /* stdin */
    dup2(i,1); /* stderr */
    dup2(i,2); /* stderr */
    close(i);
  }
}

ComboAddress g_serverControl{"127.0.0.1:5199"};

double getDoubleTime()
{						
  struct timeval now;
  gettimeofday(&now, 0);
  return 1.0*now.tv_sec + now.tv_usec/1000000.0;
}

string g_key;

void controlClientThread(int fd, ComboAddress client)
try
{
  SodiumNonce theirs, ours, readingNonce, writingNonce;
  ours.init();
  readn2(fd, (char*)theirs.value, sizeof(theirs.value));
  writen2(fd, (char*)ours.value, sizeof(ours.value));
  readingNonce.merge(ours, theirs);
  writingNonce.merge(theirs, ours);
  
  for(;;) {
    uint16_t len;
    if(!getMsgLen(fd, &len))
      break;
    char msg[len];
    readn2(fd, msg, len);
    
    string line(msg, len);
    try {
      line = sodDecryptSym(line, g_key, readingNonce);
    }
    catch (std::runtime_error& e) {
      errlog("Could not decrypt client command: %s", e.what());
      continue;
    }
    //cerr<<"Have decrypted line: "<<line<<endl;
    string response;
    try {
      std::lock_guard<std::mutex> lock(g_luamutex);
      g_outputBuffer.clear();
      auto ret=g_lua.executeCode<
	boost::optional<
	  boost::variant<
	    string
	    >
	  >
	>(line);

      // execute the supplied lua code for all the allow/report lua states
      for (auto it = g_luamultip->begin(); it != g_luamultip->end(); ++it) {
	std::lock_guard<std::mutex> lock(*(it->lua_mutexp));
	it->lua_contextp->executeCode<	
	  boost::optional<
	    boost::variant<
	      string
	      >
	    >
	  >(line);
      }

      if(ret) {
	if (const auto strValue = boost::get<string>(&*ret)) {
	  response=*strValue;
	}
      }
      else
	response=g_outputBuffer;


    }
    catch(const LuaContext::WrongTypeException& e) {
      response = "Command returned an object we can't print: " +std::string(e.what()) + "\n";
      // tried to return something we don't understand
    }
    catch(const LuaContext::ExecutionErrorException& e) {
      response = "Error: " + string(e.what()) + ": ";
      try {
        std::rethrow_if_nested(e);
      } catch(const std::exception& e) {
        // e is the exception that was thrown from inside the lambda
        response+= string(e.what());
      }
    }
    catch(const LuaContext::SyntaxErrorException& e) {
      response = "Error: " + string(e.what()) + ": ";
    }
    response = sodEncryptSym(response, g_key, writingNonce);
    putMsgLen(fd, response.length());
    writen2(fd, response.c_str(), (uint16_t)response.length());
  }
  infolog("Closed control connection from %s", client.toStringWithPort());
  close(fd);
  fd=-1;
}
catch(std::exception& e)
{
  errlog("Got an exception in client connection from %s: %s", client.toStringWithPort(), e.what());
  if(fd >= 0)
    close(fd);
}


void controlThread(int fd, ComboAddress local)
try
{
  ComboAddress client;
  int sock;
  warnlog("Accepting control connections on %s", local.toStringWithPort());
  while((sock=SAccept(fd, client)) >= 0) {
    infolog("Got control connection from %s", client.toStringWithPort());
    thread t(controlClientThread, sock, client);
    t.detach();
  }
}
catch(std::exception& e) 
{
  close(fd);
  errlog("Control connection died: %s", e.what());
}



void doClient(ComboAddress server, const std::string& command)
{
  cout<<"Connecting to "<<server.toStringWithPort()<<endl;
  int fd=socket(server.sin4.sin_family, SOCK_STREAM, 0);
  SConnect(fd, server);

  SodiumNonce theirs, ours, readingNonce, writingNonce;
  ours.init();

  writen2(fd, (const char*)ours.value, sizeof(ours.value));
  readn2(fd, (char*)theirs.value, sizeof(theirs.value));
  readingNonce.merge(ours, theirs);
  writingNonce.merge(theirs, ours);
  
  if(!command.empty()) {
    string response;
    string msg=sodEncryptSym(command, g_key, writingNonce);
    putMsgLen(fd, msg.length());
    writen2(fd, msg);
    uint16_t len;
    getMsgLen(fd, &len);
    char resp[len];
    readn2(fd, resp, len);
    msg.assign(resp, len);
    msg=sodDecryptSym(msg, g_key, readingNonce);
    cout<<msg<<endl;
    close(fd);
    return; 
  }

  set<string> dupper;
  {
    ifstream history(".history");
    string line;
    while(getline(history, line))
      add_history(line.c_str());
  }
  ofstream history(".history", std::ios_base::app);
  string lastline;
  for(;;) {
    char* sline = readline("> ");
    rl_bind_key('\t',rl_complete);
    if(!sline)
      break;

    string line(sline);
    if(!line.empty() && line != lastline) {
      add_history(sline);
      history << sline <<endl;
      history.flush();
    }
    lastline=line;
    free(sline);
    
    if(line=="quit")
      break;

    string response;
    string msg=sodEncryptSym(line, g_key, writingNonce);
    putMsgLen(fd, msg.length());
    writen2(fd, msg);
    uint16_t len;
    getMsgLen(fd, &len);
    char resp[len];
    readn2(fd, resp, len);
    msg.assign(resp, len);
    msg=sodDecryptSym(msg, g_key, readingNonce);
    cout<<msg<<endl;
  }
}

void doConsole()
{
  set<string> dupper;
  {
    ifstream history(".history");
    string line;
    while(getline(history, line))
      add_history(line.c_str());
  }
  ofstream history(".history", std::ios_base::app);
  string lastline;
  for(;;) {
    char* sline = readline("> ");
    rl_bind_key('\t',rl_complete);
    if(!sline)
      break;

    string line(sline);
    if(!line.empty() && line != lastline) {
      add_history(sline);
      history << sline <<endl;
      history.flush();
    }
    lastline=line;
    free(sline);
    
    if(line=="quit")
      break;

    string response;
    try {
      // execute the supplied lua code for all the allow/report lua states
      {
	for (auto it = g_luamultip->begin(); it != g_luamultip->end(); ++it) {
	  std::lock_guard<std::mutex> lock(*(it->lua_mutexp));
	  it->lua_contextp->executeCode<	
	    boost::optional<
	      boost::variant<
		string
		>
	      >
	    >(line);
	}
      }
      {
	std::lock_guard<std::mutex> lock(g_luamutex);
	g_outputBuffer.clear();
	auto ret=g_lua.executeCode<
	  boost::optional<
	    boost::variant<
	      string
	      >
	    >
	  >(line);
	if(ret) {
	  if (const auto strValue = boost::get<string>(&*ret)) {
	    cout<<*strValue<<endl;
	  }
	}
	else 
	  cout << g_outputBuffer;
      }
    }
    catch(const LuaContext::ExecutionErrorException& e) {
      std::cerr << e.what() << ": ";
      try {
        std::rethrow_if_nested(e);
      } catch(const std::exception& e) {
        // e is the exception that was thrown from inside the lambda
        std::cerr << e.what() << std::endl;      
      }
    }
    catch(const std::exception& e) {
      // e is the exception that was thrown from inside the lambda
      std::cerr << e.what() << std::endl;      
    }
  }
}

Json LoginTuple::to_json() const 
{
  using namespace json11;
  Json::object jattrs;
  Json::object jattrs_dev;

  for (auto& i : attrs_mv) {
    jattrs.insert(make_pair(i.first, Json(i.second)));
  }
  for (auto& i : attrs) {
    jattrs.insert(make_pair(i.first, Json(i.second)));
  }
  for (auto& i : device_attrs) {
    jattrs_dev.insert(make_pair(i.first, Json(i.second)));
  }

  return Json::object{
    {"login", login},
    {"success", success},
    {"t", (double)t}, 
    {"pwhash", pwhash},
    {"remote", remote.toString()},
    {"device_id", device_id},
    {"device_attrs", jattrs_dev},
    {"protocol", protocol},
    {"attrs", jattrs},
    {"policy_reject", policy_reject}};
}

std::string LoginTuple::serialize() const
{
  Json msg=to_json();
  return msg.dump();
}

void LoginTuple::from_json(const Json& msg)
{
  login=msg["login"].string_value();
  pwhash=msg["pwhash"].string_value();
  t=msg["t"].number_value();
  success=msg["success"].bool_value();
  remote=ComboAddress(msg["remote"].string_value());
  setLtAttrs(msg);
  setDeviceAttrs(msg);
  device_id=msg["device_id"].string_value();
  protocol=msg["protocol"].string_value();
  policy_reject=msg["policy_reject"].bool_value();
}

void LoginTuple::unserialize(const std::string& str) 
{
  string err;
  json11::Json msg=json11::Json::parse(str, err);
  from_json(msg);
}

void LoginTuple::setDeviceAttrs(const json11::Json& msg)
{
  json11::Json jattrs = msg["device_attrs"];
  if (jattrs.is_object()) {
    auto attrs_obj = jattrs.object_items();
    for (auto it=attrs_obj.begin(); it!=attrs_obj.end(); ++it) {
      string attr_name = it->first;
      if (it->second.is_string()) {
	device_attrs.insert(std::make_pair(attr_name, it->second.string_value()));
      }
    }
  }
}

void LoginTuple::setLtAttrs(const json11::Json& msg)
{
  json11::Json jattrs = msg["attrs"];
  if (jattrs.is_object()) {
    auto attrs_obj = jattrs.object_items();
    for (auto it=attrs_obj.begin(); it!=attrs_obj.end(); ++it) {
      string attr_name = it->first;
      if (it->second.is_string()) {
	attrs.insert(std::make_pair(attr_name, it->second.string_value()));
      }
      else if (it->second.is_array()) {
	auto av_list = it->second.array_items();
	std::vector<std::string> myvec;
	for (auto avit=av_list.begin(); avit!=av_list.end(); ++avit) {
	  myvec.push_back(avit->string_value());
	}
	attrs_mv.insert(std::make_pair(attr_name, myvec));
      }
    }
  }
}

Json CustomFuncArgs::to_json() const
{
  using namespace json11;
  Json::object jattrs;

  for (auto& i : attrs_mv) {
    jattrs.insert(make_pair(i.first, Json(i.second)));
  }
  for (auto& i : attrs) {
    jattrs.insert(make_pair(i.first, Json(i.second)));
  }
  return Json::object{
    {"attrs", jattrs},
      };
}

std::string CustomFuncArgs::serialize() const
{
  Json msg=to_json();
  return msg.dump();
}


 Sibling::Sibling(const ComboAddress& ca) : rem(ca), sock(ca.sin4.sin_family, SOCK_DGRAM), d_ignoreself(false)
{
  sock.connect(ca);
}

void Sibling::checkIgnoreSelf(const ComboAddress& ca)
{
  ComboAddress actualLocal;
  actualLocal.sin4.sin_family = ca.sin4.sin_family;
  socklen_t socklen = actualLocal.getSocklen();

  if(getsockname(sock.getHandle(), (struct sockaddr*) &actualLocal, &socklen) < 0) {
    return;
  }

  actualLocal.sin4.sin_port = ca.sin4.sin_port;
  if(actualLocal == rem) {
    d_ignoreself=true;
  }
}

void Sibling::send(const std::string& msg)
{
  if(d_ignoreself)
    return;
  if(::send(sock.getHandle(),msg.c_str(), msg.length(),0) <= 0)
    ++failures;
  else
    ++success;
}

std::atomic<unsigned int> g_report_sink_rr(0);
void sendReportSink(const LoginTuple& lt)
{
  auto rsinks = g_report_sinks.getLocal();
  auto msg = lt.serialize();
  auto vsize = rsinks->size();

  if (vsize == 0)
    return;

  // round-robin between report sinks
  unsigned int i = g_report_sink_rr++ % vsize;

  (*rsinks)[i]->send(msg);
}

void sendNamedReportSink(const std::string& msg)
{
  auto rsinks = g_named_report_sinks.getLocal();

  for (auto& i : *rsinks) {
    auto vsize = i.second.second.size();

    if (vsize == 0)
      continue;

    // round-robin between report sinks
    unsigned int j = (*i.second.first)++ % vsize;
    auto& vec = i.second.second;

    vec[j]->send(msg);
  }
}


GlobalStateHolder<vector<shared_ptr<Sibling>>> g_siblings;
unsigned int g_num_sibling_threads = WFORCE_NUM_SIBLING_THREADS;
SodiumNonce g_sodnonce;
std::mutex sod_mutx;

void replicateOperation(const ReplicationOperation& rep_op)
{
  auto siblings = g_siblings.getLocal();
  string msg = rep_op.serialize();
  string packet;

  {
    std::lock_guard<std::mutex> lock(sod_mutx);
    packet=g_sodnonce.toString();
    packet+=sodEncryptSym(msg, g_key, g_sodnonce);    
  }

  for(auto& s : *siblings) {
    s->send(packet);
  }
}

void receiveReplicationOperations(ComboAddress local)
{
  Socket sock(local.sin4.sin_family, SOCK_DGRAM);
  sock.bind(local);
  char buf[1500];
  ComboAddress remote=local;
  socklen_t remlen=remote.getSocklen();
  int len;
  ctpl::thread_pool p(g_num_sibling_threads);
  auto siblings = g_siblings.getLocal();
  
  for(auto& s : *siblings) {
    s->checkIgnoreSelf(local);
  }
  
  warnlog("Launched UDP sibling replication listener on %s", local.toStringWithPort());
  for(;;) {
    len=recvfrom(sock.getHandle(), buf, sizeof(buf), 0, (struct sockaddr*)&remote, &remlen);
    if(len <= 0 || len >= (int)sizeof(buf))
      continue;
    
    SodiumNonce nonce;
    memcpy((char*)&nonce, buf, crypto_secretbox_NONCEBYTES);
    string packet(buf + crypto_secretbox_NONCEBYTES, buf+len);
    string msg;
    try {
      msg=sodDecryptSym(packet, g_key, nonce);
    }
    catch (std::runtime_error& e) {
      errlog("Could not decrypt replication operation: %s", e.what());
      return;
    }
    
    p.push([msg,remote](int id) {
	ReplicationOperation rep_op;
	if (rep_op.unserialize(msg) != false) {
	  rep_op.applyOperation();
	}
	else {
	  errlog("Invalid replication operation received from %s", remote.toString());
	}
      });
  }
}

AllowReturn defaultAllowTuple(const LoginTuple& lp)
{
  // do nothing: we expect Lua function to be registered if wforce is required to actually do something
  std::vector<pair<std::string, std::string>> myvec;
  return std::make_tuple(0, "", "", myvec);
}

void defaultReportTuple(const LoginTuple& lp)
{
  // do nothing: we expect Lua function to be registered if something custom is needed
}

bool defaultReset(const std::string& type, const std::string& str_val, const ComboAddress& ca_val)
{
  return true;
}

std::string defaultCanonicalize(const std::string& login)
{
  return login;
}

allow_t g_allow{defaultAllowTuple};
report_t g_report{defaultReportTuple};
reset_t g_reset{defaultReset};
canonicalize_t g_canon{defaultCanonicalize};
CustomFuncMap g_custom_func_map;

/**** CARGO CULT CODE AHEAD ****/
extern "C" {
char* my_generator(const char* text, int state)
{
  string t(text);
  vector<string> words {"addACL",
      "addSibling(",
      "setSiblings(",
      "siblingListener(",
      "setACL",
      "showACL()",
      "shutdown()",
      "webserver",
      "controlSocket",
      "stats()",
      "siblings()",
      "newCA",
      "newNetmaskGroup",
      "makeKey",
      "setKey",
      "testCrypto",
      "showWebHooks()",
      "showCustomWebHooks()",
      "showCustomEndpoints()",
      "showNamedReportSinks()",
      "addNamedReportSink(",
      "setNamedReportSinks(",
      "showVersion()",
      "addWebHook(",
      "addCustomWebHook(",
      "setNumWebHookThreads(",
      "blacklistPersistDB(",
      "blacklistPersistReplicated()",
      "blacklistIP",
      "blacklistLogin",
      "blacklistIPLogin"
      };
  static int s_counter=0;
  int counter=0;
  if(!state)
    s_counter=0;

  for(auto w : words) {
    if(boost::starts_with(w, t) && counter++ == s_counter)  {
      s_counter++;
      return strdup(w.c_str());
    }
  }
  return 0;
}

static char** my_completion( const char * text , int start,  int end)
{
  char **matches=0;
  if (start == 0)
    matches = rl_completion_matches ((char*)text, &my_generator);
  else
    rl_bind_key('\t',rl_abort);
 
  if(!matches)
    rl_bind_key('\t', rl_abort);
  return matches;
}
}

std::string findDefaultConfigFile()
{
  std::string configFile = string(SYSCONFDIR) + "/wforce/wforce.conf";
  struct stat statbuf;

  if (stat(configFile.c_str(), &statbuf) != 0) {
    configFile = string(SYSCONFDIR) + "/wforce.conf";
  }
  return configFile;
}

struct 
{
  bool beDaemon{false};
  bool underSystemd{false};
  bool beClient{false};
  string command;
  string config;
} g_cmdLine;


int main(int argc, char** argv)
try
{
  g_stats.latency=0;
  rl_attempted_completion_function = my_completion;
  rl_completion_append_character = 0;

  signal(SIGPIPE, SIG_IGN);
  signal(SIGCHLD, SIG_IGN);
  openlog("wforce", LOG_PID, LOG_DAEMON);
  g_console=true;

#ifdef HAVE_LIBSODIUM
  if (sodium_init() == -1) {
    cerr<<"Unable to initialize crypto library"<<endl;
    exit(EXIT_FAILURE);
  }
  g_sodnonce.init();
#endif
  g_cmdLine.config = findDefaultConfigFile();
  struct option longopts[]={ 
    {"config", required_argument, 0, 'C'},
    {"execute", required_argument, 0, 'e'},
    {"client", optional_argument, 0, 'c'},
    {"systemd",  optional_argument, 0, 's'},
    {"daemon", optional_argument, 0, 'd'},
    {"help", 0, 0, 'h'}, 
    {0,0,0,0} 
  };
  int longindex=0;
  for(;;) {
    int c=getopt_long(argc, argv, ":hsdc:e:C:v", longopts, &longindex);
    if(c==-1)
      break;
    switch(c) {
    case 'C':
      g_cmdLine.config=optarg;
      break;
    case 'c':
      g_cmdLine.beClient=true;
      if (optarg) {
	g_cmdLine.config=optarg;
      }
      break;
    case ':':
      switch (optopt) {
      case 'c':
	g_cmdLine.beClient=true;
	break;
      default:
	cout << "Option '-" << (char)optopt << "' requires an argument\n";
	exit(1);
	break;
      }
      break;
    case 'd':
	g_cmdLine.beDaemon=true;
      break;
    case 's':
	g_cmdLine.underSystemd=true;
      break;
    case 'e':
      g_cmdLine.command=optarg;
      break;
    case 'h':
      cout<<"Syntax: wforce [-C,--config file] [-c,--client] [-d,--daemon] [-e,--execute cmd]\n";
      cout<<"[-h,--help] [-l,--local addr]\n";
      cout<<"\n";
      cout<<"-C,--config file      Load configuration from 'file'\n";
      cout<<"-c [file],            Operate as a client, connect to wforce, loading config from 'file' if specified\n";
      cout<<"-s,                   Operate under systemd control.\n";
      cout<<"-d,--daemon           Operate as a daemon\n";
      cout<<"-e,--execute cmd      Connect to wforce and execute 'cmd'\n";
      cout<<"-h,--help             Display this helpful message\n";
      cout<<"-l,--local address    Listen on this local address\n";
      cout<<"\n";
      exit(EXIT_SUCCESS);
      break;
    case 'v':
      g_verbose=true;
      break;
    case '?':
    default:
      cout << "Option '-" << (char)optopt << "' is invalid: ignored\n";
    }
  }
  argc-=optind;
  argv+=optind;


  if(g_cmdLine.beClient || !g_cmdLine.command.empty()) {
    setupLua(true, false, g_lua, g_allow, g_report, g_reset, g_canon, g_custom_func_map, g_cmdLine.config);
    doClient(g_serverControl, g_cmdLine.command);
    exit(EXIT_SUCCESS);
  }

  // this sets up the global lua state used for config and setup
  auto todo=setupLua(false, false, g_lua, g_allow, g_report, g_reset, g_canon, g_custom_func_map, g_cmdLine.config);

  // now we setup the allow/report lua states
  g_luamultip = std::make_shared<LuaMultiThread>(g_num_luastates);
  
  for (auto it = g_luamultip->begin(); it != g_luamultip->end(); ++it) {
    // first setup defaults in case the config doesn't specify anything
    it->allow_func = g_allow;
    it->report_func = g_report;
    it->reset_func = g_reset;
    it->canon_func = g_canon;
    setupLua(false, true, *(it->lua_contextp),
	     it->allow_func,
	     it->report_func,
	     it->reset_func,
	     it->canon_func,
	     it->custom_func_map,
	     g_cmdLine.config);
  }

  if(g_cmdLine.beDaemon) {
    g_console=false;
    daemonize();
  }
  else if (g_cmdLine.underSystemd) {
    g_console=false;
  }
  else {
    vinfolog("Running in the foreground");
  }

  for(auto& t : todo)
    t();

  auto acl = g_ACL.getCopy();
  for(auto& addr : {"127.0.0.0/8", "10.0.0.0/8", "100.64.0.0/10", "169.254.0.0/16", "192.168.0.0/16", "172.16.0.0/12", "::1/128", "fc00::/7", "fe80::/10"})
    acl.addMask(addr);
  g_ACL.setState(acl);

  vector<string> vec;
  std::string acls;
  acl.toStringVector(&vec);
  for(const auto& s : vec) {
    if (!acls.empty())
      acls += ", ";
    acls += s;
  }
  noticelog("ACL allowing queries from: %s", acls.c_str());

  // setup blacklist_db purge thread
  thread t1(BlackListDB::purgeEntriesThread, &g_bl_db);
  t1.detach();

  // start the performance stats thread
  startStatsThread();

  // load the persistent blacklist entries
  if (!g_bl_db.loadPersistEntries()) {
    errlog("Could not load persistent DB entries, please fix configuration or check redis availability. Exiting.");
    exit(1);
  }
  
#ifdef HAVE_LIBSYSTEMD
  sd_notify(0, "READY=1");
#endif

  if(!(g_cmdLine.beDaemon || g_cmdLine.underSystemd)) {
    doConsole();
  } 
  else {
    while (true)
      pause();
  }
  _exit(EXIT_SUCCESS);

}
catch(std::exception &e)
{
  errlog("Fatal error: %s", e.what());
}
