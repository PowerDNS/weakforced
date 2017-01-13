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

#pragma once
#include "ext/luawrapper/include/LuaContext.hpp"
#include <time.h>
#include "misc.hh"
#include "iputils.hh"
#include <atomic>
#include <boost/circular_buffer.hpp>
#include <mutex>
#include <thread>
#include "sholder.hh"
#include "sstuff.hh"
#include "replication.hh"
#include "webhook.hh"
#include "ext/json11/json11.hpp"
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/identity.hpp>
#include <boost/multi_index/sequenced_index.hpp>

struct WForceStats
{
  using stat_t=std::atomic<uint64_t>;
  stat_t reports{0};
  stat_t allows{0};
  stat_t denieds{0};
  double latency{0};
  
};

extern struct WForceStats g_stats;

struct ClientState
{
  ComboAddress local;
  int udpFD;
  int tcpFD;
};

extern std::mutex g_luamutex;
extern LuaContext g_lua;
extern std::string g_outputBuffer; // locking for this is ok, as locked by g_luamutex (functions using g_outputBuffer MUST NOT be enabled for the allow/report lua contexts)

void receiveReports(ComboAddress local);
void receiveReplicationOperations(ComboAddress local);
struct Sibling
{
  explicit Sibling(const ComboAddress& rem);
  Sibling(const Sibling&) = delete;
  ComboAddress rem;
  Socket sock;
  std::atomic<unsigned int> success{0};
  std::atomic<unsigned int> failures{0};
  void send(const std::string& msg);
  void checkIgnoreSelf(const ComboAddress& ca);
  bool d_ignoreself{false};
};

extern GlobalStateHolder<NetmaskGroup> g_ACL;
extern GlobalStateHolder<vector<shared_ptr<Sibling>>> g_siblings;
extern ComboAddress g_sibling_listen;
extern ComboAddress g_serverControl; // not changed during runtime

extern std::string g_key; // in theory needs locking

struct dnsheader;

void controlThread(int fd, ComboAddress local);
void dnsdistWebserverThread(int sock, const ComboAddress& local, const string& password);
bool getMsgLen(int fd, uint16_t* len);
bool putMsgLen(int fd, uint16_t len);
void* tcpAcceptorThread(void* p);
double getDoubleTime();

struct LoginTuple
{
  double t;
  ComboAddress remote;
  string login;
  string pwhash;
  string device_id;
  std::map<std::string, std::string> device_attrs;
  string protocol;
  bool success;
  std::map<std::string, std::string> attrs; // additional attributes
  std::map<std::string, std::vector<std::string>> attrs_mv; // additional multi-valued attributes
  bool policy_reject;
  Json to_json() const;
  std::string serialize() const;
  void from_json(const Json& msg);
  void unserialize(const std::string& src);
  void setLtAttrs(const json11::Json& msg);
  void setDeviceAttrs(const json11::Json& msg);

  bool operator<(const LoginTuple& r) const
  {
    if(std::tie(t, login, pwhash, success) < std::tie(r.t, r.login, r.pwhash, r.success))
      return true;
    ComboAddress cal(remote);
    ComboAddress car(r.remote);
    cal.sin4.sin_port=0;
    car.sin4.sin_port=0;
    return cal < car;
  }
};

struct CustomFuncArgs {
  std::map<std::string, std::string> attrs; // additional attributes
  std::map<std::string, std::vector<std::string>> attrs_mv; // additional multi-valued attributes

  void setAttrs(const json11::Json& msg) {
    LoginTuple lt;
    lt.setLtAttrs(msg);
    attrs = std::move(lt.attrs);
    attrs_mv = std::move(lt.attrs_mv);
  }
  std::string serialize() const;
  Json to_json() const;
};
typedef std::vector<std::pair<std::string, std::string>> KeyValVector;

extern GlobalStateHolder<vector<shared_ptr<Sibling>>> g_report_sinks;
extern GlobalStateHolder<std::map<std::string, std::pair<std::shared_ptr<std::atomic<unsigned int>>, std::vector<std::shared_ptr<Sibling>>>>> g_named_report_sinks;
void sendReportSink(const LoginTuple& lt);
void sendNamedReportSink(const std::string& msg);

typedef std::tuple<int, std::string, std::string, KeyValVector> AllowReturn;
typedef std::tuple<bool, KeyValVector> CustomFuncReturn;
enum CustomReturnFields { customRetStatus=0, customRetAttrs=1 };

typedef std::function<AllowReturn(const LoginTuple&)> allow_t;
extern allow_t g_allow;
typedef std::function<void(const LoginTuple&)> report_t;
extern report_t g_report;
typedef std::function<bool(const std::string&, const std::string&, const ComboAddress&)> reset_t;
extern reset_t g_reset;
typedef std::function<std::string(const std::string&)> canonicalize_t;
typedef std::function<CustomFuncReturn(const CustomFuncArgs&)> custom_func_t;
typedef std::map<std::string, std::pair<custom_func_t, bool>> CustomFuncMap;
extern CustomFuncMap g_custom_func_map;

vector<std::function<void(void)>> setupLua(bool client, bool allow_report, LuaContext& c_lua, allow_t& allow_func, report_t& report_func, reset_t& reset_func, canonicalize_t& canon_func, CustomFuncMap& custom_func_map, const std::string& config);

struct LuaThreadContext {
  std::shared_ptr<LuaContext> lua_contextp;
  std::shared_ptr<std::mutex> lua_mutexp;
  allow_t allow_func;
  report_t report_func;
  reset_t reset_func;
  canonicalize_t canon_func;
  CustomFuncMap custom_func_map;
};

#define NUM_LUA_STATES 6

class LuaMultiThread
{
public:
  LuaMultiThread() : num_states(NUM_LUA_STATES),
		     state_index(0)
  {
    LuaMultiThread(num_states);
  }

  LuaMultiThread(unsigned int nstates) : num_states(nstates),
					 state_index(0)
  {
    for (unsigned int i=0; i<num_states; i++) {
      LuaThreadContext ltc;
      ltc.lua_contextp = std::make_shared<LuaContext>();
      ltc.lua_mutexp = std::make_shared<std::mutex>();
      lua_cv.push_back(ltc);
    }	
  }

  // these are used to setup the allow and report function pointers
  std::vector<LuaThreadContext>::iterator begin() { return lua_cv.begin(); }
  std::vector<LuaThreadContext>::iterator end() { return lua_cv.end(); }

  bool reset(const std::string& type, const std::string& login_value, const ComboAddress& ca_value) {
    auto lt_context = getLuaState();
    // lock the lua state mutex
    std::lock_guard<std::mutex> lock(*(lt_context.lua_mutexp));
    // call the reset function
    return lt_context.reset_func(type, login_value, ca_value);
  }

  AllowReturn allow(const LoginTuple& lt) {
    auto lt_context = getLuaState();
    // lock the lua state mutex
    std::lock_guard<std::mutex> lock(*(lt_context.lua_mutexp));
    // call the allow function
    return lt_context.allow_func(lt);
  }

  void report(const LoginTuple& lt) {
    auto lt_context = getLuaState();
    // lock the lua state mutex
    std::lock_guard<std::mutex> lock(*(lt_context.lua_mutexp));
    // call the report function
    lt_context.report_func(lt);
  }

  std::string canonicalize(const std::string& login) {
    auto lt_context = getLuaState();
    // lock the lua state mutex
    std::lock_guard<std::mutex> lock(*(lt_context.lua_mutexp));
    // call the canonicalize function
    return lt_context.canon_func(login);
  }

  CustomFuncReturn custom_func(const std::string& command, const CustomFuncArgs& cfa, bool& reportSink) {
    auto lt_context = getLuaState();
    // lock the lua state mutex
    std::lock_guard<std::mutex> lock(*(lt_context.lua_mutexp));
    // call the custom function
    for (const auto& i : lt_context.custom_func_map) {
      if (command.compare(i.first) == 0) {
	reportSink = i.second.second;
	return i.second.first(cfa);
      }
    }
    return CustomFuncReturn(false, KeyValVector{});
  }
  
protected:
  LuaThreadContext& getLuaState()
  {
    std::lock_guard<std::mutex> lock(mutx);
    if (state_index >= num_states)
      state_index = 0;
    return lua_cv[state_index++];
  }
private:
  std::vector<LuaThreadContext> lua_cv;
  unsigned int num_states;
  unsigned int state_index;
  std::mutex mutx;
};

extern std::shared_ptr<LuaMultiThread> g_luamultip;
extern int g_num_luastates;
extern unsigned int g_num_worker_threads;
#define WFORCE_NUM_WORKER_THREADS 4
extern unsigned int g_num_sibling_threads;
#define WFORCE_NUM_SIBLING_THREADS 2

extern WebHookRunner g_webhook_runner;
extern WebHookDB g_webhook_db;
extern WebHookDB g_custom_webhook_db;
