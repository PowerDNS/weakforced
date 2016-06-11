#pragma once
#include <time.h>
#include "misc.hh"
#include "iputils.hh"
#include <atomic>
#include <mutex>
#include <thread>
#include <random>
#include "sholder.hh"
#include "sstuff.hh"
#include "dolog.hh"
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/thread/thread_time.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/date_time/posix_time/posix_time_io.hpp>

struct BlackListEntry {
  std::string key;
  std::string reason;
  boost::system_time expiration;
  bool operator<(const BlackListEntry& r) const
  {
    if (key < r.key) return true;
    return false;
  }
};

using namespace boost::multi_index;

enum BLType { IP_BL=0, LOGIN_BL=1, IP_LOGIN_BL=2 };

class BlackListDB {
public:  
  BlackListDB() {}
  BlackListDB(const BlackListDB&) = delete;

  void addEntry(const ComboAddress& ca, time_t seconds, const std::string& reason);
  void addEntry(const std::string& login, time_t seconds, const std::string& reason);
  void addEntry(const ComboAddress& ca, const std::string& login, time_t seconds, const std::string& reason);

  bool checkEntry(const ComboAddress& ca);
  bool checkEntry(const std::string& login);
  bool checkEntry(const ComboAddress& ca, const std::string& login);

  bool getEntry(const ComboAddress& ca, BlackListEntry& ret);
  bool getEntry(const std::string& login, BlackListEntry& ret);
  bool getEntry(const ComboAddress& ca, const std::string& login, BlackListEntry& ret);

  bool deleteEntry(const ComboAddress& ca);
  bool deleteEntry(const std::string& login);
  bool deleteEntry(const ComboAddress& ca, const std::string& login);

  time_t getExpiration(const ComboAddress& ca);
  time_t getExpiration(const std::string& login);
  time_t getExpiration(const ComboAddress& ca, const std::string& login);

  void purgeEntries();
  static void purgeEntriesThread(BlackListDB* bl_db)
  {
    bl_db->purgeEntries();
  }
  std::vector<BlackListEntry> getIPEntries();
  std::vector<BlackListEntry> getLoginEntries();
  std::vector<BlackListEntry> getIPLoginEntries();
private:
  struct TimeTag{};
  struct KeyTag{};
  struct SeqTag{};
  typedef multi_index_container<
    BlackListEntry,
    indexed_by<
      ordered_non_unique<
	tag<TimeTag>,
	member<BlackListEntry, boost::system_time, &BlackListEntry::expiration>	>,
      hashed_unique<
	tag<KeyTag>,
	member<BlackListEntry, std::string, &BlackListEntry::key> >,
      sequenced<tag<SeqTag>>
    >
    > blacklist_t;

  const char* bl_names[3] = { "ip_bl", "login_bl", "ip_login_bl" };
  const char* key_names[3] = { "ip", "login", "ip_login" };
  blacklist_t ip_blacklist;
  blacklist_t login_blacklist;
  blacklist_t ip_login_blacklist;
  std::mutex mutx;

  void _addEntry(const std::string& key, time_t seconds, blacklist_t& blacklist, const std::string& reason);
  bool _checkEntry(const std::string& key, blacklist_t& blacklist);
  bool _getEntry(const std::string& key, blacklist_t& blacklist, BlackListEntry& ret_ble);
  bool _deleteEntry(const std::string& key, blacklist_t& blacklist);
  time_t _getExpiration(const std::string& key, blacklist_t& blacklist); // returns number of seconds until expiration
  void _purgeEntries(BLType blt, blacklist_t& blacklist);
  void addEntryLog(BLType blt, const std::string& key, time_t seconds, const std::string& reason);
  void deleteEntryLog(BLType blt, const std::string& key);
  void expireEntryLog(BLType blt, const std::string& key);
  std::string ipLoginStr(const ComboAddress& ca, const std::string& login);
};

extern BlackListDB bl_db;
