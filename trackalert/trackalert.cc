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
#include <stddef.h>
#define SYSLOG_NAMES
#include <syslog.h>
#include "trackalert.hh"
#include "sstuff.hh"
#include "misc.hh"
#include <netinet/tcp.h>
#include <limits>
#include "dolog.hh"
#include <readline/readline.h>
#include <readline/history.h>
#include "base64.hh"
#include <fstream>
#include "json11.hpp"
#include <unistd.h>
#include <sys/stat.h>
#include "sodcrypto.hh"
#include "perf-stats.hh"
#include "trackalert-luastate.hh"
#include "webhook.hh"
#include "lock.hh"
#include "trackalert-web.hh"
#include "ext/threadname.hh"
#include "prometheus.hh"

#include <getopt.h>
#ifdef HAVE_LIBSYSTEMD
#include <systemd/sd-daemon.h>
#endif
#include "device_parser.hh"
#include "wforce_ns.hh"

using std::atomic;
using std::thread;
bool g_verbose=false;

struct TrackalertStats g_stats;
bool g_console=false;
bool g_docker=false;
LogLevel g_loglevel{LogLevel::Info};

string g_outputBuffer;

WebHookRunner g_webhook_runner;
WebHookDB g_webhook_db;
WebHookDB g_custom_webhook_db;
WforceWebserver g_webserver;
CustomFuncMap g_custom_func_map;
curlTLSOptions g_curl_tls_options;

std::string g_configDir; // where the config files are located
bool g_configurationDone = false;

// The Scheduler class should be thread-safe
std::shared_ptr<Bosma::Scheduler> g_bg_schedulerp;
int g_num_scheduler_threads = NUM_SCHEDULER_THREADS;


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

ComboAddress g_serverControl{"127.0.0.1:4005"};

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
  Socket sock(fd);
  SodiumNonce theirs, ours, readingNonce, writingNonce;
  ours.init();
  readn2(fd, (char*)theirs.value, sizeof(theirs.value));
  writen2(fd, (char*)ours.value, sizeof(ours.value));
  readingNonce.merge(ours, theirs);
  writingNonce.merge(theirs, ours);

  setThreadName("wf/ctrl-client");

  sock.setKeepAlive();

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
      return;
    }
    //cerr<<"Have decrypted line: "<<line<<endl;
    string response;
    try {
      // execute the supplied lua code for all the allow/report lua states
      for (auto it = g_luamultip->begin(); it != g_luamultip->end(); ++it) {
	std::lock_guard<std::mutex> lock((*it)->lua_mutex);
        (*it)->lua_context.executeCode<
	  boost::optional<
	    boost::variant<
	      string
	      >
	    >
	  >(line);
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
	    response=*strValue;
	  }
	}
	else
	  response=g_outputBuffer;
      }
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
  // The Socket class wrapper will close the socket for us
  infolog("Closed control connection from %s", client.toStringWithPort());
}
catch(const std::exception& e)
{
  errlog("Got an exception in client connection from %s: %s", client.toStringWithPort(), e.what());
}


void controlThread(int fd, ComboAddress local)
try
{
  ComboAddress client;
  int sock;

  setThreadName("wf/ctrl-accept");

  noticelog("Accepting control connections on %s", local.toStringWithPort());
  while((sock=SAccept(fd, client)) >= 0) {
    infolog("Got control connection from %s", client.toStringWithPort());
    thread t(controlClientThread, sock, client);
    t.detach();
  }
}
catch(const std::exception& e) 
{
  close(fd);
  errlog("Control connection died: %s", e.what());
}

void doClient(ComboAddress server, const std::string& command)
{
  cout<<"Connecting to "<<server.toStringWithPort()<<endl;
  int fd=socket(server.sin4.sin_family, SOCK_STREAM, 0);
  if (fd < 0) {
    cout << "Could not open socket" << endl;
    return;
  }
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
	  std::lock_guard<std::mutex> lock((*it)->lua_mutex);
	(*it)->lua_context.executeCode<
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


SodiumNonce g_sodnonce;
std::mutex sod_mutx;

void defaultReportTuple(const LoginTuple& lp)
{
  // do nothing: we expect Lua function to be registered if something custom is needed
}

void defaultBackground()
{
  // do nothing: we expect Lua function to be registered if something custom is needed
}

report_t g_report{defaultReportTuple};
background_t g_background{defaultBackground};

/**** CARGO CULT CODE AHEAD ****/
extern "C" {
char* my_generator(const char* text, int state)
{
  string t(text);
  vector<string> words {"addACL",
      "setACL",
      "showACL()",
      "shutdown()",
      "webserver",
      "controlSocket",
      "stats()",
      "newCA",
      "newNetmaskGroup",
      "makeKey",
      "reloadGeoIPDBs()",
      "setKey",
      "testCrypto",
      "showCustomWebHooks()",
      "showPerfStats()",
      "showCommandStats()",
      "showCustomStats()",
      "showVersion()",
      "showCustomEndpoints()",
      "setCustomEndpoint(",
      "addWebHook(",
      "addCustomWebHook(",
      "setNumWebHookThreads("
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
  std::string configFile = string(SYSCONFDIR) + "/wforce/trackalert.conf";
  struct stat statbuf;

  if (stat(configFile.c_str(), &statbuf) != 0) {
    g_configDir = string(SYSCONFDIR);
    configFile = string(SYSCONFDIR) + "/trackalert.conf";
  }
  else
    g_configDir = string(SYSCONFDIR) + "/wforce";
  return configFile;
}

struct 
{
  bool beDaemon{false};
  bool underSystemd{false};
  bool underDocker{false};
  bool beClient{false};
  string command;
  string config;
  unsigned int facility{LOG_DAEMON};
} g_cmdLine;

int main(int argc, char** argv)
try
{
  rl_attempted_completion_function = my_completion;
  rl_completion_append_character = 0;

  signal(SIGPIPE, SIG_IGN);
  signal(SIGCHLD, SIG_IGN);
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
    {"regexes", required_argument, 0, 'R'},
    {"execute", required_argument, 0, 'e'},
    {"client", optional_argument, 0, 'c'},
    {"systemd",  optional_argument, 0, 's'},
    {"daemon", optional_argument, 0, 'd'},
    {"docker", optional_argument, 0, 'D'},
    {"facility", required_argument, 0, 'f'},
    {"loglevel", required_argument, 0, 'l'},
    {"help", 0, 0, 'h'}, 
    {0,0,0,0} 
  };
  int longindex=0;
  for(;;) {
    int c=getopt_long(argc, argv, ":hsdDc:e:C:R:f:l:v", longopts, &longindex);
    if(c==-1)
      break;
    switch(c) {
    case 'C':
      g_cmdLine.config=getFileFromPath(optarg);
      g_configDir = getDirectoryPath(optarg);
      break;
    case 'c':
      g_cmdLine.beClient=true;
      if (optarg) {
	g_cmdLine.config=getFileFromPath(optarg);
	g_configDir = getDirectoryPath(optarg);
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
    case 'D':
      g_cmdLine.underDocker=true;
      g_docker = true;
      break;
    case 's':
      g_cmdLine.underSystemd=true;
      break;
    case 'e':
      g_cmdLine.command=optarg;
      break;
    case 'f':
      int i;
      for (i = 0; facilitynames[i].c_name; i++)
        if (strcmp((char *)facilitynames[i].c_name, optarg)==0)
          break;
      if (facilitynames[i].c_name) {
        g_cmdLine.facility = facilitynames[i].c_val;
      }
      else {
        cout << "Bad log facility" << endl;
        exit(1);
        break;
      }
      break;
    case 'l':
        try {
          g_loglevel = static_cast<LogLevel>(std::stoi(optarg));
        }
        catch (const std::invalid_argument &ia) {
          cout << "Bad log level (" << optarg << ") - must be an integer" << endl;
          exit(1);
        }
        break;
    case 'h':
      cout<<"Syntax: wforce [-C,--config file] [-c,--client] [-d,--daemon] [-e,--execute cmd]\n";
      cout<<"[-h,--help] [-l,--local addr]\n";
      cout<<"\n";
      cout<<"-C,--config file      Load configuration from 'file'\n";
      cout<<"-c [file],            Operate as a client, connect to wforce, loading config from 'file' if specified\n";
      cout<<"-s,                   Operate under systemd control.\n";
      cout<<"-d,--daemon           Operate as a daemon\n";
      cout<<"-D,--docker           Enable logging for docker\n";
      cout<<"-e,--execute cmd      Connect to wforce and execute 'cmd'\n";
      cout<<"-f,--facility name    Use log facility 'name'\n";
      cout<<"-l,--loglevel level   Log level as an integer. 0 is Emerg, 7 is Debug. Defaults to 6 (Info).\n";
      cout<<"-h,--help             Display this helpful message\n";
      cout<<"\n";
      exit(EXIT_SUCCESS);
      break;
    case 'v':
      g_verbose=true;
      g_loglevel=LogLevel::Debug;
      break;
    case '?':
    default:
      cout << "Option '-" << (char)optopt << "' is invalid: ignored\n";
    }
  }
  argc-=optind;
  argv+=optind;

  g_webserver.setWebLogLevel(g_loglevel);
  openlog("trackalert", LOG_PID, LOG_DAEMON);
  
  g_singleThreaded = false;

  // start background scheduler
  g_bg_schedulerp = std::make_shared<Bosma::Scheduler>(g_num_scheduler_threads);
  
  if (chdir(g_configDir.c_str()) != 0) {
    warnlog("Could not change working directory to %s (%s)", g_configDir, strerror(errno));
  }
  
  if(g_cmdLine.beClient || !g_cmdLine.command.empty()) {
    setupLua(true, false, g_lua, g_report, nullptr, g_custom_func_map, g_cmdLine.config);
    doClient(g_serverControl, g_cmdLine.command);
    exit(EXIT_SUCCESS);
  }

  // Initialise Prometheus Metrics
  initPrometheusMetrics(std::make_shared<PrometheusMetrics>("trackalert"));

  // now we setup the multi-lua lua states (we do this first because there are routines such as the scheduler
  // in the global config that use the multi-lua states)
  g_luamultip = std::make_shared<LuaMultiThread>(g_num_luastates);

  // this sets up the global lua state used for config and setup
  auto todo=setupLua(false, false, g_lua, g_report, nullptr, g_custom_func_map, g_cmdLine.config);
  
  for (auto it = g_luamultip->begin(); it != g_luamultip->end(); ++it) {
    // first setup defaults in case the config doesn't specify anything
    (*it)->report_func = g_report;
    setupLua(false, true, (*it)->lua_context,
      (*it)->report_func,
      &((*it)->bg_func_map),
      (*it)->custom_func_map,
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

  g_webhook_runner.startThreads();

  // register all the webserver commands
  registerWebserverCommands();
  
  auto acl = g_webserver.getACL();
  for(auto& addr : {"127.0.0.0/8", "10.0.0.0/8", "100.64.0.0/10", "169.254.0.0/16", "192.168.0.0/16", "172.16.0.0/12", "::1/128", "fc00::/7", "fe80::/10"})
    acl.addMask(addr);
  g_webserver.setACL(acl);

  vector<string> vec;
  std::string acls;
  acl.toStringVector(&vec);
  for(const auto& s : vec) {
    if (!acls.empty())
      acls += ", ";
    acls += s;
  }
  noticelog("ACL allowing queries from: %s", acls.c_str());

  // start the performance stats thread
  startStatsThread();

  // start the threads created by lua setup. Includes the webserver accept thread
  for(auto& t : todo)
    t();
  
#ifdef HAVE_LIBSYSTEMD
  sd_notify(0, "READY=1");
#endif

  g_configurationDone = true;
  
  if(!(g_cmdLine.beDaemon || g_cmdLine.underSystemd || g_cmdLine.underDocker)) {
    doConsole();
  } 
  else {
    while (true)
      pause();
  }
  _exit(EXIT_SUCCESS);

 }
catch(const LuaContext::ExecutionErrorException& e) {
  try {
    errlog("Fatal Lua error: %s", e.what());
    std::rethrow_if_nested(e);
  }
  catch(const std::exception& e) {
    errlog("Details: %s", e.what());
  }
  catch(WforceException &ae)
    {
      errlog("Fatal wforce error: %s", ae.reason);
    }
  _exit(EXIT_FAILURE);
 }
 catch(std::exception &e) {
   errlog("Fatal error: %s", e.what());
 }
 catch(WforceException &ae) {
   errlog("Fatal wforce error: %s", ae.reason);
   _exit(EXIT_FAILURE);
 }
