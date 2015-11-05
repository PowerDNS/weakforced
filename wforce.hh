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


struct Rings {
  Rings()
  {
    clientRing.set_capacity(10000);
    
  }
  boost::circular_buffer<ComboAddress> clientRing;
};

extern Rings g_rings; // XXX locking for this is still substandard, queryRing and clientRing need RW lock

struct ClientState
{
  ComboAddress local;
  int udpFD;
  int tcpFD;
};


extern std::mutex g_luamutex;
extern LuaContext g_lua;
extern std::string g_outputBuffer; // locking for this is ok, as locked by g_luamutex
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
vector<std::function<void(void)>> setupLua(bool client, const std::string& config);
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

using namespace boost::multi_index;

class WForceDB
{
public:
  WForceDB() {}
  WForceDB(const WForceDB&) = delete;
  void reportTuple(const LoginTuple& lp);

  int countFailures(const ComboAddress& remote, int seconds) const;
  int countDiffFailures(const ComboAddress& remote, int seconds) const;
  int countDiffFailures(const ComboAddress& remote, string login, int seconds) const;
  void timePurge(int second);
  void numberPurge(int amount);
  std::vector<LoginTuple> getTuples() const;
  std::vector<LoginTuple> getTuplesLogin(const std::string& login) const;
  std::vector<LoginTuple> getTuplesRemote(const ComboAddress& remote) const;
  size_t size();
  void clear();
  int clearLogin(const std::string& login);
  int clearRemote(const ComboAddress& remote);
private:
  struct TimeTag{};
  struct LoginTag{};
  struct RemoteTag{};
  struct RemoteLoginTag{};
  struct SeqTag{};
  
  typedef multi_index_container<
    LoginTuple,
    indexed_by<
      ordered_unique<identity<LoginTuple> >,
      ordered_non_unique<
        tag<TimeTag>,
        member<LoginTuple, double, &LoginTuple::t>
      >,
      ordered_non_unique<
        tag<LoginTag>,
        member<LoginTuple, string, &LoginTuple::login>
      >,
      ordered_non_unique<
        tag<RemoteTag>,
        member<LoginTuple, ComboAddress, &LoginTuple::remote>, ComboAddress::addressOnlyLessThan
	>,
      sequenced<tag<SeqTag>>

    >
  > storage_t;

  storage_t d_logins;
  mutable std::mutex d_mutex;
};
void spreadReport(const LoginTuple& lt);
int allowTupleDefault(const WForceDB* wfd, const LoginTuple& lp);
extern WForceDB g_wfdb;
typedef std::function<int(const WForceDB*, const LoginTuple&)> allow_t;
extern allow_t g_allow;
