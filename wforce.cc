/*
    PowerDNS Versatile Database Driven Nameserver
    Copyright (C) 2013 - 2015  PowerDNS.COM BV

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License version 2
    as published by the Free Software Foundation

    Additionally, the license of this program contains a special
    exception which allows to distribute the program in binary form when
    it is linked against OpenSSL.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
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
#include "sodcrypto.hh"
#include <getopt.h>
#ifdef WITH_SYSTEMDSYSTEMUNITDIR
#include <systemd/sd-daemon.h>
#endif

using std::atomic;
using std::thread;
bool g_verbose;

struct WForceStats g_stats;
bool g_console;

GlobalStateHolder<NetmaskGroup> g_ACL;
string g_outputBuffer;
vector<ComboAddress> g_locals;

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
  SodiumNonce theirs;
  readn2(fd, (char*)theirs.value, sizeof(theirs.value));
  SodiumNonce ours;
  ours.init();
  writen2(fd, (char*)ours.value, sizeof(ours.value));

  for(;;) {
    uint16_t len;
    if(!getMsgLen(fd, &len))
      break;
    char msg[len];
    readn2(fd, msg, len);
    
    string line(msg, len);
    line = sodDecryptSym(line, g_key, theirs);
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

      if(ret) {
	if (const auto strValue = boost::get<string>(&*ret)) {
	  response=*strValue;
	}
      }
      else
	response=g_outputBuffer;


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
    response = sodEncryptSym(response, g_key, ours);
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
    warnlog("Got control connection from %s", client.toStringWithPort());
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

  SodiumNonce theirs, ours;
  ours.init();

  writen2(fd, (const char*)ours.value, sizeof(ours.value));
  readn2(fd, (char*)theirs.value, sizeof(theirs.value));

  if(!command.empty()) {
    string response;
    string msg=sodEncryptSym(command, g_key, ours);
    putMsgLen(fd, msg.length());
    writen2(fd, msg);
    uint16_t len;
    getMsgLen(fd, &len);
    char resp[len];
    readn2(fd, resp, len);
    msg.assign(resp, len);
    msg=sodDecryptSym(msg, g_key, theirs);
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
    string msg=sodEncryptSym(line, g_key, ours);
    putMsgLen(fd, msg.length());
    writen2(fd, msg);
    uint16_t len;
    getMsgLen(fd, &len);
    char resp[len];
    readn2(fd, resp, len);
    msg.assign(resp, len);
    msg=sodDecryptSym(msg, g_key, theirs);
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

static void bindAny(int af, int sock)
{
  int one = 1;

#ifdef IP_FREEBIND
  if (setsockopt(sock, IPPROTO_IP, IP_FREEBIND, &one, sizeof(one)) < 0)
    warnlog("Warning: IP_FREEBIND setsockopt failed: %s", strerror(errno));
#endif

#ifdef IP_BINDANY
  if (af == AF_INET)
    if (setsockopt(sock, IPPROTO_IP, IP_BINDANY, &one, sizeof(one)) < 0)
      warnlog("Warning: IP_BINDANY setsockopt failed: %s", strerror(errno));
#endif
#ifdef IPV6_BINDANY
  if (af == AF_INET6)
    if (setsockopt(sock, IPPROTO_IPV6, IPV6_BINDANY, &one, sizeof(one)) < 0)
      warnlog("Warning: IPV6_BINDANY setsockopt failed: %s", strerror(errno));
#endif
#ifdef SO_BINDANY
  if (setsockopt(sock, SOL_SOCKET, SO_BINDANY, &one, sizeof(one)) < 0)
    warnlog("Warning: SO_BINDANY setsockopt failed: %s", strerror(errno));
#endif
}

std::string LoginTuple::serialize() const
{
  using namespace json11;
  Json msg=Json::object{
    {"login", login},
    {"success", success},
    {"t", (double)t}, 
    {"pwhash", pwhash},
    {"remote", remote.toString()}};
  return msg.dump();
}

void LoginTuple::unserialize(const std::string& str) 
{
  using namespace json11;
  string err;
  Json msg=Json::parse(str, err);
  login=msg["login"].string_value();
  pwhash=msg["pwhash"].string_value();
  t=msg["t"].number_value();
  success=msg["success"].bool_value();
  remote=ComboAddress(msg["remote"].string_value());
}

 Sibling::Sibling(const ComboAddress& ca) : rem(ca), sock(ca.sin4.sin_family, SOCK_DGRAM), d_ignoreself(false)
{
  sock.connect(ca);
  ComboAddress actualLocal;
  actualLocal.sin4.sin_family = ca.sin4.sin_family;
  socklen_t socklen = actualLocal.getSocklen();

  if(getsockname(sock.getHandle(), (struct sockaddr*) &actualLocal, &socklen) < 0) {
    return;
  }

  actualLocal.sin4.sin_port = ca.sin4.sin_port;
  if(actualLocal == ca) {
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

GlobalStateHolder<vector<shared_ptr<Sibling>>> g_siblings;

SodiumNonce g_sodnonce;

void spreadReport(const LoginTuple& lt)
{
  auto siblings = g_siblings.getLocal();
  string msg=lt.serialize();

  string packet =g_sodnonce.toString();
  packet+=sodEncryptSym(msg, g_key, g_sodnonce);

  for(auto& s : *siblings) {
    s->send(packet);
  }
}

void receiveReports(ComboAddress local)
{
  Socket sock(local.sin4.sin_family, SOCK_DGRAM);
  sock.bind(local);
  char buf[1500];
  ComboAddress remote=local;
  socklen_t remlen=remote.getSocklen();
  int len;
  infolog("Launched UDP sibling listener on %s", local.toStringWithPort());
  for(;;) {
    len=recvfrom(sock.getHandle(), buf, sizeof(buf), 0, (struct sockaddr*)&remote, &remlen);
    if(len <= 0 || len >= (int)sizeof(buf))
      continue;

    SodiumNonce nonce;
    memcpy((char*)&nonce, buf, crypto_secretbox_NONCEBYTES);
    string packet(buf + crypto_secretbox_NONCEBYTES, buf+len);
    
    string msg=sodDecryptSym(packet, g_key, nonce);

    LoginTuple lt;
    lt.unserialize(msg);
    vinfolog("Got a report from sibling %s: %s,%s,%s,%f", remote.toString(), lt.login,lt.pwhash,lt.remote.toString(),lt.t);
    {
      std::lock_guard<std::mutex> lock(g_luamutex);
      g_report(lt);
    }
  }
}

int defaultAllowTuple(const LoginTuple& lp)
{
  // do nothing: we expect Lua function to be registered if wforce is required to actually do something
  return 0;
}

void defaultReportTuple(const LoginTuple& lp)
{
  // do nothing: we expect Lua function to be registered if something custom is needed
}

std::function<int(const LoginTuple&)> g_allow{defaultAllowTuple};
std::function<void(const LoginTuple&)> g_report{defaultReportTuple};

/**** CARGO CULT CODE AHEAD ****/
extern "C" {
char* my_generator(const char* text, int state)
{
  string t(text);
  vector<string> words {"addACL",
      "addSibling(",
      "clearDB()",
      "setSiblings(",
      "showLogin(",
      "showRemote(",
      "clearLogin(",
      "clearRemote(",
      "siblingListener(",
      "addLocal",
      "setACL",
      "showACL()",
      "shutdown()",
      "webserver",
      "controlSocket",
      "report",
      "stats()",
      "siblings",
      "allow",
      "countFailures",
      "countDiffFailures",
      "newCA",
      "newNetmaskGroup",
      "setAllow",
      "makeKey",
      "setKey",
      "testCrypto"
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

struct 
{
  vector<string> locals;
  bool beDaemon{false};
  bool underSystemd{false};
  bool beClient{false};
  string command;
  string config;
} g_cmdLine;


/* spawn as many of these as required, they call Accept on a socket on which they will accept queries, and 
   they will hand off to worker threads & spawn more of them if required
*/
void* tcpAcceptorThread(void* p)
{
  ClientState* cs = (ClientState*) p;

  ComboAddress remote;
  remote.sin4.sin_family = cs->local.sin4.sin_family;
  
  for(;;) {
    try {
      int fd = SAccept(cs->tcpFD, remote);

      vinfolog("Got connection from %s", remote.toStringWithPort());
      writen2(fd, "220 Unimplemented\r\n");
      close(fd);
      // now what
    }
    catch(...){}
  }

  return 0;
}


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
  g_cmdLine.config=string(SYSCONFDIR) + "/wforce.conf";
  struct option longopts[]={ 
    {"config", required_argument, 0, 'C'},
    {"execute", required_argument, 0, 'e'},
    {"client", optional_argument, 0, 'c'},
    {"local",  required_argument, 0, 'l'},
    {"daemon", optional_argument, 0, 'd'},
    {"help", 0, 0, 'h'}, 
    {0,0,0,0} 
  };
  int longindex=0;
  for(;;) {
    int c=getopt_long(argc, argv, "hsdce:C:l:m:v", longopts, &longindex);
    if(c==-1)
      break;
    switch(c) {
    case 'C':
      g_cmdLine.config=optarg;
      break;
    case 'c':
      g_cmdLine.beClient=true;
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
      cout<<"-c,                   Operate as a client, connect to wforce\n";
      cout<<"-s,                   Operate under systemd control.\n";
      cout<<"-d,--daemon           Operate as a daemon\n";
      cout<<"-e,--execute cmd      Connect to wforce and execute 'cmd'\n";
      cout<<"-h,--help             Display this helpful message\n";
      cout<<"-l,--local address    Listen on this local address\n";
      cout<<"\n";
      exit(EXIT_SUCCESS);
      break;
    case 'l':
      g_cmdLine.locals.push_back(optarg);
      break;
    case 'v':
      g_verbose=true;
      break;
    }
  }
  argc-=optind;
  argv+=optind;


  if(g_cmdLine.beClient || !g_cmdLine.command.empty()) {
    setupLua(true, g_cmdLine.config);
    doClient(g_serverControl, g_cmdLine.command);
    exit(EXIT_SUCCESS);
  }

  auto todo=setupLua(false, g_cmdLine.config);

  if(g_cmdLine.locals.size()) {
    g_locals.clear();
    for(auto loc : g_cmdLine.locals)
      g_locals.push_back(ComboAddress(loc, 4000));
  }
  
  if(g_locals.empty())
    g_locals.push_back(ComboAddress("0.0.0.0", 4000));
  

  vector<ClientState*> toLaunch;
  for(const auto& local : g_locals) {
    ClientState* cs = new ClientState;
    cs->local= local;
    cs->udpFD = SSocket(cs->local.sin4.sin_family, SOCK_DGRAM, 0);
    if(cs->local.sin4.sin_family == AF_INET6) {
      SSetsockopt(cs->udpFD, IPPROTO_IPV6, IPV6_V6ONLY, 1);
    }
    //if(g_vm.count("bind-non-local"))
    bindAny(local.sin4.sin_family, cs->udpFD);

    //    setSocketTimestamps(cs->udpFD);

    if(IsAnyAddress(local)) {
      int one=1;
      setsockopt(cs->udpFD, IPPROTO_IP, GEN_IP_PKTINFO, &one, sizeof(one));     // linux supports this, so why not - might fail on other systems
#ifdef IPV6_RECVPKTINFO
      setsockopt(cs->udpFD, IPPROTO_IPV6, IPV6_RECVPKTINFO, &one, sizeof(one)); 
#endif
    }

    SBind(cs->udpFD, cs->local);    
    toLaunch.push_back(cs);
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


  for(const auto& local : g_locals) {
    ClientState* cs = new ClientState;
    cs->local= local;

    cs->tcpFD = SSocket(cs->local.sin4.sin_family, SOCK_STREAM, 0);

    SSetsockopt(cs->tcpFD, SOL_SOCKET, SO_REUSEADDR, 1);
#ifdef TCP_DEFER_ACCEPT
    SSetsockopt(cs->tcpFD, SOL_TCP,TCP_DEFER_ACCEPT, 1);
#endif
    if(cs->local.sin4.sin_family == AF_INET6) {
      SSetsockopt(cs->tcpFD, IPPROTO_IPV6, IPV6_V6ONLY, 1);
    }
    //    if(g_vm.count("bind-non-local"))
      bindAny(cs->local.sin4.sin_family, cs->tcpFD);
    SBind(cs->tcpFD, cs->local);
    SListen(cs->tcpFD, 64);
    warnlog("Listening on %s",cs->local.toStringWithPort());
    
    thread t1(tcpAcceptorThread, cs);
    t1.detach();
  }

#ifdef WITH_SYSTEMDSYSTEMUNITDIR
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
