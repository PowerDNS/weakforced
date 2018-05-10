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
#include "blacklist.hh"
#include "perf-stats.hh"
#include "luastate.hh"
#include "webhook.hh"
#include "lock.hh"
#include "wforce-web.hh"
#include "twmap-wrapper.hh"
#include "replication_sdb.hh"
#include "minicurl.hh"
#include "iputils.hh"

#include <getopt.h>
#ifdef HAVE_LIBSYSTEMD
#include <systemd/sd-daemon.h>
#endif
#include "ext/ctpl.h"
#include "device_parser.hh"
#include "wforce_ns.hh"

using std::atomic;
using std::thread;
bool g_verbose=false;

struct WForceStats g_stats;
bool g_console;

string g_outputBuffer;

WebHookRunner g_webhook_runner;
WebHookDB g_webhook_db;
WebHookDB g_custom_webhook_db;
WforceWebserver g_webserver;
syncData g_sync_data;

std::string g_configDir; // where the config files are located
std::shared_ptr<UserAgentParser> g_ua_parser_p;

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

ComboAddress g_serverControl{"127.0.0.1:4004"};

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

void encryptMsg(const std::string& msg, std::string& packet)
{
    std::lock_guard<std::mutex> lock(sod_mutx);
    packet=g_sodnonce.toString();
    packet+=sodEncryptSym(msg, g_key, g_sodnonce);
}

bool decryptMsg(const char* buf, int len, std::string& msg)
{
  SodiumNonce nonce;
  memcpy((char*)&nonce, buf, crypto_secretbox_NONCEBYTES);
  string packet(buf + crypto_secretbox_NONCEBYTES, buf+len);
  try {
    msg=sodDecryptSym(packet, g_key, nonce);
  }
  catch (std::runtime_error& e) {
    errlog("Could not decrypt replication operation: %s", e.what());
    return false;
  }
  return true;
}

Json callWforceGetURL(const std::string& url, const std::string& password, std::string& err)
{
  MiniCurl mc;
  MiniCurlHeaders mch;
  mc.setTimeout(5);
  mch.insert(std::make_pair("Authorization", "Basic " + Base64Encode(std::string("wforce") + ":" + password)));
  std::string get_result = mc.getURL(url, mch);
  return Json::parse(get_result, err);
}

Json callWforcePostURL(const std::string& url, const std::string& password, const std::string& post_body, std::string& err)
{
  MiniCurl mc;
  MiniCurlHeaders mch;
  std::string post_res, post_err;
  
  mc.setTimeout(5);
  mch.insert(std::make_pair("Authorization", "Basic " + Base64Encode(std::string("wforce") + ":" + password)));
  mch.insert(std::make_pair("Content-Type", "application/json"));
  if (mc.postURL(url, post_body, mch, post_res, post_err)) {
    return Json::parse(post_res, err);
  }
  else {
    err = post_err;
    return Json();
  }
}

void syncDBThread(const ComboAddress& ca, const std::string& callback_url,
                  const std::string& callback_pw)
{
  Sibling sibling(ca);
  unsigned int num_synced = 0;
  
  noticelog("Synchronizing DBs to: %s, will notify on callback url: %s",
            ca.toStringWithPort(), callback_url);

  try {
    Socket rep_sock(ca.sin4.sin_family, SOCK_STREAM, 0);
    rep_sock.connect(ca);
  
    // loop through the DBs
    std::map<std::string, TWStringStatsDBWrapper> my_dbmap;
    {
      std::lock_guard<std::mutex> lock(dbMap_mutx);
      // copy (this is safe - everything important is in a shared ptr)
      my_dbmap = dbMap;
    }
    for (auto& i : dbMap) {
      TWStringStatsDBWrapper sdb = i.second;
      std::string db_name = i.first;
      for (auto it = sdb.startDBDump(); it != sdb.DBDumpIteratorEnd(); ++it) {
        try {
          TWStatsDBDumpEntry entry;
          std::string key;
          if (sdb.DBDumpEntry(it, entry, key)) {
            std::shared_ptr<SDBReplicationOperation> sdb_rop = std::make_shared<SDBReplicationOperation>(db_name, SDBOperation_SDBOpType_SDBOpSyncKey, key, entry);
            ReplicationOperation rep_op(sdb_rop, WforceReplicationMsg_RepType_SDBType);
            string msg = rep_op.serialize();
            string packet;
            encryptMsg(msg, packet);
            uint16_t nsize = htons(packet.length());
            rep_sock.write((char*)&nsize, sizeof(nsize));
            rep_sock.writen(packet);
            num_synced++;
          }
        }
        catch(const NetworkError& e) {
          sdb.endDBDump();
          auto eptr = std::current_exception();
          std::rethrow_exception(eptr);
        }
        catch(const WforceException& e) {
          sdb.endDBDump();
          auto eptr = std::current_exception();
          std::rethrow_exception(eptr);
        }
        catch(const std::exception& e) {
          sdb.endDBDump();
          auto eptr = std::current_exception();
          std::rethrow_exception(eptr);
        }
      }
      sdb.endDBDump();
    }
    infolog("Synchronizing DBs to: %s was completed. Synced %d entries.", ca.toStringWithPort(), num_synced);
  }
  catch (NetworkError& e) {
    errlog("Synchronizing DBs to: %s did not complete. [Network Error: %s]", ca.toStringWithPort(), e.what());
  }
  catch(const WforceException& e) {
    errlog("Synchronizing DBs to: %s did not complete. [Wforce Error: %s]", ca.toStringWithPort(), e.reason);
  }
  catch (const std::exception& e) {
    errlog("Synchronizing DBs to: %s did not complete. [exception Error: %s]", ca.toStringWithPort(), e.what());
  }
  // Once we've finished replicating we need to let the requestor know we're
  // done by calling the callback URL
  std::string err;
  Json msg = callWforceGetURL(callback_url, callback_pw, err);
  if (msg.is_null()) {
    errlog("Synchronizing DBs callback to: %s failed due to no parseable result returned [Error: %s]", callback_url, err);
  }
  else {
    if (!msg["status"].is_null()) {
      std::string status = msg["status"].string_value();
      if (status == std::string("ok")) {
        noticelog("Synchronizing DBs callback to: %s was successful", callback_url);
        return;
      }
    }
    errlog("Synchronizing DBs callback to: %s was unsuccessful (no status=ok in result)", callback_url);
  }
}

unsigned int checkHostUptime(const std::string& url, const std::string& password)
{
  unsigned int ret_uptime = 0;
  std::string err;
  Json msg = callWforceGetURL(url, password, err);
  if (!msg.is_null()) {
    if (!msg["uptime"].is_null()) {
      ret_uptime = msg["uptime"].int_value();
      infolog("checkSyncHosts: uptime: %d returned from sync host: %s", ret_uptime, url);
    }
    else {
      errlog("checkSyncHosts: No uptime in response from sync host: %s", url);
    }
  }
  else {
    errlog("checkSyncHosts: No valid response from sync host: %s [Error: %s]", url, err);
  }
  return ret_uptime;
}

void checkSyncHosts()
{
  bool found_sync_host = false;
  for (auto i : g_sync_data.sync_hosts) {
    std::string sync_host = i.first.toStringWithPort();
    std::string password = i.second;
    std::string stats_url = "http://" + sync_host + "/?command=stats";
    unsigned int uptime = checkHostUptime(stats_url, password);
    if (uptime > g_sync_data.min_sync_host_uptime) {
      // we have a winner, maybe
      std::string err;
      std::string sync_url = "http://" + sync_host + "/?command=syncDBs";
      std::string callback_url = "http://" + g_sync_data.webserver_listen_addr.toStringWithPort() + "/?command=syncDone";
      Json post_json = Json::object{{"replication_host", g_sync_data.sibling_listen_addr.toString()},
                                    {"replication_port", ntohs(g_sync_data.sibling_listen_addr.sin4.sin_port)},
                                    {"callback_url", callback_url},
                                    {"callback_auth_pw", g_sync_data.webserver_password}};
      Json msg = callWforcePostURL(sync_url, password, post_json.dump(), err);
      if (!msg.is_null()) {
        if (!msg["status"].is_null()) {
          std::string status = msg["status"].string_value();
          if (status == "ok") {
            found_sync_host = true;
            infolog("checkSyncHosts: Successful request to synchronize DBs with sync host: %s", sync_host);
            break;
          }
          else {
            errlog("checkSyncHosts: Error returned status=%s from sync_host: %s", status, sync_url);
          }
        }
        else {
          errlog("checkSyncHosts: No status in response from sync host: %s", sync_url);
        }
      }
      else {
        errlog("checkSyncHosts: No valid response from sync host: %s [Error: %s]", sync_url, err);
      }
    }
  }
  // If we didn't find a sync host we can't warm up
  if (found_sync_host == false)
    g_ping_up = true;
}

void replicateOperation(const ReplicationOperation& rep_op)
{
  auto siblings = g_siblings.getLocal();
  string msg = rep_op.serialize();
  string packet;

  encryptMsg(msg, packet);

  for(auto& s : *siblings) {
    s->send(packet);
  }
}

bool checkConnFromSibling(const ComboAddress& remote,
                          shared_ptr<Sibling>& recv_sibling)
{
  auto siblings = g_siblings.getLocal();

  for (auto &s : *siblings) {
    if (ComboAddress::addressOnlyEqual()(s->rem, remote) == true) {
      recv_sibling = s;
      break;
    }
  }

  if (recv_sibling == nullptr) {
    errlog("Message received from host (%s) that is not configured as a sibling", remote.toStringWithPort());
    return false;
  }
  return true;
}

void parseReceivedReplicationMsg(const std::string& msg, const ComboAddress& remote, std::shared_ptr<Sibling> recv_sibling)
{
  static ctpl::thread_pool p(g_num_sibling_threads);
  
  p.push([msg,remote,recv_sibling](int id) {
      ReplicationOperation rep_op;
      if (rep_op.unserialize(msg) != false) {
        rep_op.applyOperation();
        recv_sibling->rcvd_success++;
      }
      else {
        errlog("Invalid replication operation received from %s", remote.toString());
        recv_sibling->rcvd_fail++;
      }
    });
}

void parseTCPReplication(std::shared_ptr<Socket> sockp, const ComboAddress& remote, std::shared_ptr<Sibling> recv_sibling)
{
  infolog("New TCP Replication connection from %s", remote.toString());
  uint16_t size;
  unsigned char ssize = sizeof(size);
  char buffer[65535];
  int len;
  unsigned int num_rcvd=0;
  
  try {
    while(true) {
      len = sockp->read((char*)&size, ssize);
      if (len != ssize) {
        if (len)
          errlog("parseTCPReplication: error reading size, got %d bytes, but was expecting %d bytes", len, ssize);
        break;
      }
      size = ntohs(size);
      if (!size) {
        errlog("parseTCPReplication: Zero-length size field");
        break;
      }
      if (size > sizeof(buffer)) {
        errlog("parseTCPReplication: This should not happen - asked to read more than 65535 bytes");
        break;
      }
      int size_read = 0;
      if ((size_read = sockp->readAll(buffer, size)) != size) {
        errlog("parseTCPReplication: Told to read %d bytes, but actually read %d bytes", size, size_read);
        break;
      }
      std::string msg;
      if (!decryptMsg(buffer, size_read, msg)) {
        recv_sibling->rcvd_fail++;
        continue;
      }
      parseReceivedReplicationMsg(msg, remote, recv_sibling);
      num_rcvd++;
    }
    infolog("Received %d Replication entries from %s", num_rcvd, remote.toString());
  }
  catch(std::exception& e) {
    errlog("ParseTCPReplication: client thread died with error: %s", e.what());
  }
}

void receiveReplicationOperationsTCP(ComboAddress local)
{
  Socket sock(local.sin4.sin_family, SOCK_STREAM, 0);
  ComboAddress remote=local;

  sock.setReuseAddr();
  sock.bind(local);
  sock.listen(1024);
  warnlog("Launched TCP sibling replication listener on %s", local.toStringWithPort());
  
  for (;;) {
    // we wait for activity
    try {
      shared_ptr<Sibling> recv_sibling = nullptr;
      std::shared_ptr<Socket> connp(sock.accept());
      if (connp->getRemote(remote) && !checkConnFromSibling(remote, recv_sibling)) {
        continue;
      }
      thread t1(parseTCPReplication, connp, remote, recv_sibling);
      t1.detach();
    }
    catch(std::exception& e) {
      errlog("receiveReplicationOperationsTCP: error accepting new connection: %s", e.what());
    }
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
  auto siblings = g_siblings.getLocal();
  
  for(auto& s : *siblings) {
    s->checkIgnoreSelf(local);
  }
  
  warnlog("Launched UDP sibling replication listener on %s", local.toStringWithPort());
  for(;;) {
    shared_ptr<Sibling> recv_sibling = nullptr;
    len=recvfrom(sock.getHandle(), buf, sizeof(buf), 0, (struct sockaddr*)&remote, &remlen);
    if(len <= 0 || len >= (int)sizeof(buf))
      continue;

    if (!checkConnFromSibling(remote, recv_sibling)) {
      continue;
    }
    
    string msg;
    
    if (!decryptMsg(buf, len, msg)) {
      recv_sibling->rcvd_fail++;
      continue;
    }
    parseReceivedReplicationMsg(msg, remote, recv_sibling);
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
      "showPerfStats()",
      "showCommandStats()",
      "showCustomStats()",
      "showStringStatsDB()",
      "showVersion()",
      "addWebHook(",
      "addCustomWebHook(",
      "setNumWebHookThreads(",
      "blacklistPersistDB(",
      "blacklistPersistReplicated()",
      "blacklistNetmask",
      "blacklistIP",
      "blacklistLogin",
      "blacklistIPLogin",
      "reloadGeoIPDBs()"
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
    g_configDir = string(SYSCONFDIR);
    configFile = string(SYSCONFDIR) + "/wforce.conf";
  }
  else
    g_configDir = string(SYSCONFDIR) + "/wforce";
  return configFile;
}

std::string findUaRegexFile()
{
  return(g_configDir + "/regexes.yaml");
}

void checkUaRegexFile(const std::string& regexFile)
{
  struct stat statbuf;
  
  if (stat(regexFile.c_str(), &statbuf) != 0) {
    errlog("Fatal error: cannot find regexes.yaml at %d", regexFile);
    exit(-1);
  }
}

struct 
{
  bool beDaemon{false};
  bool underSystemd{false};
  bool beClient{false};
  string command;
  string config;
  string regexes;
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
  g_cmdLine.regexes = findUaRegexFile();
  struct option longopts[]={ 
    {"config", required_argument, 0, 'C'},
    {"regexes", required_argument, 0, 'R'},
    {"execute", required_argument, 0, 'e'},
    {"client", optional_argument, 0, 'c'},
    {"systemd",  optional_argument, 0, 's'},
    {"daemon", optional_argument, 0, 'd'},
    {"help", 0, 0, 'h'}, 
    {0,0,0,0} 
  };
  int longindex=0;
  for(;;) {
    int c=getopt_long(argc, argv, ":hsdc:e:C:R:v", longopts, &longindex);
    if(c==-1)
      break;
    switch(c) {
    case 'C':
      g_cmdLine.config=getFileFromPath(optarg);
      g_configDir = getDirectoryPath(optarg);
      break;
    case 'R':
      g_cmdLine.regexes=optarg;
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
    case 's':
      g_cmdLine.underSystemd=true;
      break;
    case 'e':
      g_cmdLine.command=optarg;
      break;
    case 'h':
      cout<<"Syntax: wforce [-C,--config file] [-R,--regexes file] [-c,--client] [-d,--daemon] [-e,--execute cmd]\n";
      cout<<"[-h,--help] [-l,--local addr]\n";
      cout<<"\n";
      cout<<"-C,--config file      Load configuration from 'file'\n";
      cout<<"-R,--regexes file     Load User-Agent regexes from 'file'\n";
      cout<<"-c [file],            Operate as a client, connect to wforce, loading config from 'file' if specified\n";
      cout<<"-s,                   Operate under systemd control.\n";
      cout<<"-d,--daemon           Operate as a daemon\n";
      cout<<"-e,--execute cmd      Connect to wforce and execute 'cmd'\n";
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

  g_singleThreaded = false;
  if (chdir(g_configDir.c_str()) != 0) {
    warnlog("Could not change working directory to %s (%s)", g_configDir, strerror(errno));
  }
  
  if (!g_cmdLine.beClient) {
    checkUaRegexFile(g_cmdLine.regexes);
    vinfolog("Will read UserAgent regexes from %s", g_cmdLine.regexes);
    g_ua_parser_p = std::make_shared<UserAgentParser>(g_cmdLine.regexes);
  }
  
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
    (*it)->allow_func = g_allow;
    (*it)->report_func = g_report;
    (*it)->reset_func = g_reset;
    (*it)->canon_func = g_canon;
    setupLua(false, true, (*it)->lua_context,
	     (*it)->allow_func,
	     (*it)->report_func,
	     (*it)->reset_func,
	     (*it)->canon_func,
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
  
  // start the threads created by lua setup. Includes the webserver accept thread
  for(auto& t : todo)
    t();

  // Loop through the list of configured sync hosts, check if any have been
  // up long enough and if so, kick off a DB sync operation to fill our DBs
  checkSyncHosts();
  
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
