#pragma once
#include "ext/luawrapper/include/LuaContext.hpp"
#include <time.h>
#include "misc.hh"
#include "iputils.hh"
#include <atomic>
#include <boost/circular_buffer.hpp>
#include <mutex>
#include <thread>
#include <random>
#include "sholder.hh"
#include "sstuff.hh"
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
struct Sibling
{
  explicit Sibling(const ComboAddress& rem);
  Sibling(const Sibling&) = delete;
  ComboAddress rem;
  Socket sock;
  std::atomic<unsigned int> success{0};
  std::atomic<unsigned int> failures{0};
  void send(const std::string& msg);
  bool d_ignoreself{false};
};

extern GlobalStateHolder<NetmaskGroup> g_ACL;
extern GlobalStateHolder<vector<shared_ptr<Sibling>>> g_siblings;
extern ComboAddress g_serverControl; // not changed during runtime

extern std::vector<ComboAddress> g_locals; // not changed at runtime
extern std::string g_key; // in theory needs locking

struct dnsheader;

void controlThread(int fd, ComboAddress local);
template<typename T, typename... Args>
std::unique_ptr<T> make_unique(Args&&... args)
{
    return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}
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
  bool success;
  std::map<std::string, std::string> attrs; // additional attributes
  std::map<std::string, std::vector<std::string>> attrs_mv; // additional multi-valued attributes
  std::string serialize() const;
  void unserialize(const std::string& src);
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

typedef std::tuple<int, std::string, std::string, std::vector<pair<std::string, std::string>>> AllowReturn;

vector<std::function<void(void)>> setupLua(bool client, bool allow_report, LuaContext& c_lua, std::function<AllowReturn(const LoginTuple&)>& allow_func, std::function<void(const LoginTuple&)>& report_func, const std::string& config);

void spreadReport(const LoginTuple& lt);
typedef std::function<AllowReturn(const LoginTuple&)> allow_t;
extern allow_t g_allow;
typedef std::function<void(const LoginTuple&)> report_t;
extern report_t g_report;

struct LuaThreadContext {
  std::shared_ptr<LuaContext> lua_contextp;
  std::shared_ptr<std::mutex> lua_mutexp;
  std::function<AllowReturn(const LoginTuple&)> allow_func;
  std::function<void(const LoginTuple&)> report_func;
};

#define NUM_LUA_STATES 1

class LuaMultiThread
{
public:
  LuaMultiThread() : rng(std::random_device()()),
		     num_states(NUM_LUA_STATES)
  {
    LuaMultiThread(num_states);
  }

  LuaMultiThread(unsigned int nstates) : rng(std::random_device()()),
					 num_states(nstates)
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
    // call the allow function
    lt_context.report_func(lt);
  }
protected:
  LuaThreadContext& getLuaState()
  {
    int s;
    { 
      std::lock_guard<std::mutex> lock(mutx);
      std::uniform_int_distribution<int> uni(0, num_states-1);
      s = uni(rng);
    }
    // randomly select a lua state
    return lua_cv[s];
  }
private:
  std::mt19937 rng;
  std::vector<LuaThreadContext> lua_cv;
  unsigned int num_states;
  std::mutex mutx;
};

extern std::shared_ptr<LuaMultiThread> g_luamultip;
extern int g_num_luastates;
extern unsigned int g_num_worker_threads;
#define WFORCE_NUM_WORKER_THREADS 4
extern unsigned int g_num_sibling_threads;
#define WFORCE_NUM_SIBLING_THREADS 2
