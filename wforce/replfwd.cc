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
#include "sstuff.hh"
#include "misc.hh"
#include <netinet/tcp.h>
#include <limits>
#include <thread>
#include "dolog.hh"
#include <readline/readline.h>
#include <readline/history.h>
#include "base64.hh"
#include <fstream>
#include <unistd.h>
#include <sys/stat.h>
#include "sodcrypto.hh"
#include "lock.hh"
#include "replfwd.hh"
#include "replfwd-web.hh"
#include "replfwd-replication.hh"
#include "wforce-prometheus.hh"
#include "iputils.hh"
#include "ext/threadname.hh"

#include <getopt.h>
#ifdef HAVE_LIBSYSTEMD
#include <systemd/sd-daemon.h>
#endif
#include "device_parser.hh"
#include "wforce_ns.hh"
#include "wforce-replication.hh"

using std::atomic;
using std::thread;
bool g_verbose=false;
bool g_console=true;
bool g_docker=false;

struct WForceStats g_stats;
WforceWebserver g_webserver;
WebHookRunner g_webhook_runner;
WebHookDB g_webhook_db;
string g_outputBuffer;
LogLevel g_loglevel{LogLevel::Info};

std::string g_configDir; // where the config files are located
ReplfwdReplication g_replication; //  handling replication

struct
{
  bool beDaemon{false};
  bool underSystemd{false};
  bool beClient{false};
  string command;
  string config;
  string regexes;
  unsigned int facility{LOG_DAEMON};
} g_cmdLine;

std::mutex g_luamutex;
LuaContext g_lua;

void replicateOperation(const ReplicationOperation& rep_op)
{
  g_replication.replicateOperation(rep_op);
}

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

std::string findDefaultConfigFile()
{
  std::string configFile = string(SYSCONFDIR) + "/wforce/replfwd.conf";
  struct stat statbuf;

  if (stat(configFile.c_str(), &statbuf) != 0) {
    g_configDir = string(SYSCONFDIR);
    configFile = string(SYSCONFDIR) + "/replfwd.conf";
  }
  else
    g_configDir = string(SYSCONFDIR) + "/wforce";
  return configFile;
}

int main(int argc, char** argv)
try
{
  g_stats.latency=0;

  signal(SIGPIPE, SIG_IGN);
  signal(SIGCHLD, SIG_IGN);

#ifdef HAVE_LIBSODIUM
  if (sodium_init() == -1) {
    cerr<<"Unable to initialize crypto library"<<endl;
    exit(EXIT_FAILURE);
  }
#endif
  g_cmdLine.config = findDefaultConfigFile();
  struct option longopts[]={ 
    {"config", required_argument, 0, 'C'},
    {"systemd",  optional_argument, 0, 's'},
    {"daemon", optional_argument, 0, 'd'},
    {"docker", optional_argument, 0, 'D'},
    {"facility", required_argument, 0, 'f'},
    {"key", optional_argument, 0, 'k'},
    {"loglevel", required_argument, 0, 'l'},
    {"help", 0, 0, 'h'}, 
    {0,0,0,0} 
  };
  int longindex=0;
  for(;;) {
    int c=getopt_long(argc, argv, ":hsdDkC:f:l:v", longopts, &longindex);
    if(c==-1)
      break;
    switch(c) {
    case 'C':
      g_cmdLine.config=getFileFromPath(optarg);
      g_configDir = getDirectoryPath(optarg);
      break;
    case 'd':
      g_console = false;
      g_cmdLine.beDaemon=true;
      break;
    case 's':
      g_console = false;
      g_cmdLine.underSystemd=true;
      break;
    case 'D':
      g_docker = true;
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
    case 'k':
      {
        std::string newkey{newKey()};
        cout << newkey << endl;
        exit(EXIT_SUCCESS);
        break;
      }
    case 'h':
      cout<<"Syntax: replfwd [-C,--config file] [-D, --docker] [-d,--daemon] [-f,--facility-name facility] [-l --loglevel 0-7] [-k,--key] [-h,--help]\n";
      cout<<"\n";
      cout<<"-C,--config file      Load configuration from 'file'\n";
      cout<<"-s,                   Operate under systemd control.\n";
      cout<<"-d,--daemon           Operate as a daemon\n";
      cout<<"-D,--docker           Enable timestamp logging for docker\n";
      cout<<"-f,--facility name    Use log facility 'name'\n";
      cout<<"-l,--loglevel level   Log level as an integer. 0 is Emerg, 7 is Debug. Defaults to 6 (Info).\n";
      cout<<"-k,--key              Create and output a key, then exit\n";
      cout<<"-h,--help             Display this helpful message\n";
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

  g_webserver.setWebLogLevel(g_loglevel);
  openlog("replfwd", LOG_PID, g_cmdLine.facility);
  
  g_singleThreaded = false;
  if (chdir(g_configDir.c_str()) != 0) {
    warnlog("Could not change working directory to %s (%s)", g_configDir, strerror(errno));
  }

  // Initialise Prometheus Metrics
  initWforcePrometheusMetrics(std::make_shared<WforcePrometheus>("replfwd"));

  // Setup a sensible ACL, but allow it to be overridden by Lua if necessary
  auto acl = g_webserver.getACL();
  for(auto& addr : {"127.0.0.0/8", "10.0.0.0/8", "100.64.0.0/10", "169.254.0.0/16", "192.168.0.0/16", "172.16.0.0/12", "::1/128", "fc00::/7", "fe80::/10"})
    acl.addMask(addr);
  g_webserver.setACL(acl);

  // this sets up the global lua state used for config and setup
  auto todo=setupLua(g_lua, g_cmdLine.config);

  if(g_cmdLine.beDaemon) {
    daemonize();
  }
  else {
    vinfolog("Running in the foreground");
  }

  // register all the webserver commands
  registerWebserverCommands();

  vector<string> vec;
  std::string acls;
  acl.toStringVector(&vec);
  for(const auto& s : vec) {
    if (!acls.empty())
      acls += ", ";
    acls += s;
  }
  noticelog("ACL allowing queries from: %s", acls.c_str());

  // Start the replication worker threads
  g_replication.startReplicationWorkerThreads();

  // start the threads created by lua setup. Includes the webserver accept thread
  for(auto& t : todo)
    t();

#ifdef HAVE_LIBSYSTEMD
  sd_notify(0, "READY=1");
#endif

  while (true)
    pause();
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
      errlog("Fatal replfwd error: %s", ae.reason);
    }
  _exit(EXIT_FAILURE);
 }
 catch(std::exception &e) {
   errlog("Fatal error: %s", e.what());
 }
 catch(WforceException &ae) {
   errlog("Fatal replfwd error: %s", ae.reason);
   _exit(EXIT_FAILURE);
 }
