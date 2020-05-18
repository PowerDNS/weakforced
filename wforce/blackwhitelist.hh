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

#pragma once
#include <time.h>
#include "misc.hh"
#include "lock.hh"
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
#include <hiredis/hiredis.h>
#include "ext/threadname.hh"
#include "webhook.hh"

struct BlackWhiteListEntry {
  std::string key;
  std::string reason;
  boost::system_time expiration;
  bool operator<(const BlackWhiteListEntry& r) const
  {
    if (key < r.key) return true;
    return false;
  }
};

using namespace boost::multi_index;

enum BLWLType { IP_BLWL=0, LOGIN_BLWL=1, IP_LOGIN_BLWL=2, NONE_BLWL=999 };
enum class BLWLDBType { BLACKLIST=0, WHITELIST=1 };

class BlackWhiteListDB {
public:  
  BlackWhiteListDB(BLWLDBType type) {
    db_type = type;
    if (type == BLWLDBType::BLACKLIST) {
      list_names = { "ip_bl", "login_bl", "ip_login_bl" };
      redis_prefix = "wfbl";
      ip_ret_msg = "Temporarily blacklisted IP Address - try again later";
      login_ret_msg = "Temporarily blacklisted Login Name - try again later";
      iplogin_ret_msg = "Temporarily blacklisted IP/Login Tuple - try again later";
    }
    else {
      list_names = { "ip_wl", "login_wl", "ip_login_wl" };
      redis_prefix = "wfwl";
      ip_ret_msg = "Whitelisted IP Address";
      login_ret_msg = "Whitelisted Login Name";
      iplogin_ret_msg = "Whitelisted IP/Login Tuple";
    }
    redis_context = NULL;
    redis_port = 6379;
    redis_timeout=1;
    redis_rw_timeout_usecs = 100000;
  }
  BlackWhiteListDB(const BlackWhiteListDB&) = delete;

  void addEntry(const Netmask& nm, time_t seconds, const std::string& reason);
  void addEntry(const ComboAddress& ca, time_t seconds, const std::string& reason);
  void addEntry(const std::string& login, time_t seconds, const std::string& reason);
  void addEntry(const ComboAddress& ca, const std::string& login, time_t seconds, const std::string& reason);

  bool checkEntry(const ComboAddress& ca) const;
  bool checkEntry(const std::string& login) const;
  bool checkEntry(const ComboAddress& ca, const std::string& login) const;

  bool getEntry(const ComboAddress& ca, BlackWhiteListEntry& ret) const;
  bool getEntry(const std::string& login, BlackWhiteListEntry& ret) const;
  bool getEntry(const ComboAddress& ca, const std::string& login, BlackWhiteListEntry& ret) const;

  void deleteEntry(const Netmask& nm);
  void deleteEntry(const ComboAddress& ca);
  void deleteEntry(const std::string& login);
  void deleteEntry(const ComboAddress& ca, const std::string& login);

  time_t getExpiration(const ComboAddress& ca) const;
  time_t getExpiration(const std::string& login) const;
  time_t getExpiration(const ComboAddress& ca, const std::string& login) const;

  void addEntryInternal(const std::string& key, time_t seconds, BLWLType bl_type, const std::string& reason, bool replicate);
  void deleteEntryInternal(const std::string& key, BLWLType bl_type, bool replicate);

  void makePersistent(const std::string& host, unsigned int port);
  void persistReplicated() { persist_replicated = true; }
  bool loadPersistEntries();

  void purgeEntries();

  static void purgeEntriesThread(BlackWhiteListDB* blwl_db)
  {
    setThreadName("wf/blwl-purge");
    blwl_db->purgeEntries();
  }

  std::vector<BlackWhiteListEntry> getIPEntries() const;
  std::vector<BlackWhiteListEntry> getLoginEntries() const;
  std::vector<BlackWhiteListEntry> getIPLoginEntries() const;

  void setConnectTimeout(int timeout);
  void setRWTimeout(int timeout_secs, int timeout_usecs);

  const std::string& getIPRetMsg() const { return ip_ret_msg;}
  const std::string& getLoginRetMsg() const { return login_ret_msg; }
  const std::string& getIPLoginRetMsg() const { return iplogin_ret_msg; }
  void setIPRetMsg(const std::string& msg) { ip_ret_msg = msg; }
  void setLoginRetMsg(const std::string& msg) { login_ret_msg = msg; }
  void setIPLoginRetMsg(const std::string& msg) { iplogin_ret_msg = msg; }
  
private:
  struct TimeTag{};
  struct KeyTag{};
  struct SeqTag{};
  typedef multi_index_container<
    BlackWhiteListEntry,
    indexed_by<
      ordered_non_unique<
	tag<TimeTag>,
	member<BlackWhiteListEntry, boost::system_time, &BlackWhiteListEntry::expiration>	>,
      hashed_unique<
	tag<KeyTag>,
	member<BlackWhiteListEntry, std::string, &BlackWhiteListEntry::key> >,
      sequenced<tag<SeqTag>>
    >
    > blackwhitelist_t;

  std::vector<std::string> list_names = { "ip_blwl", "login_blwl", "ip_login_blwl" };
  const char* key_names[4] = { "ip", "login", "ip_login", NULL };
  NetmaskGroup iplist_netmask;
  blackwhitelist_t ip_list;
  blackwhitelist_t login_list;
  blackwhitelist_t ip_login_list;
  mutable pthread_rwlock_t rwlock = PTHREAD_RWLOCK_INITIALIZER;
  bool persist=false;
  bool persist_replicated=false;
  std::string redis_server;
  unsigned int redis_port;
  redisContext* redis_context;
  std::atomic<int> redis_timeout;
  std::atomic<int> redis_rw_timeout_secs;
  std::atomic<int> redis_rw_timeout_usecs;
  std::string redis_prefix = {"wfbl"};
  BLWLDBType db_type;
  std::string ip_ret_msg;
  std::string login_ret_msg;
  std::string iplogin_ret_msg;
  
  void _addEntry(const std::string& key, time_t seconds, blackwhitelist_t& blackwhitelist, const std::string& reason);
  bool _checkEntry(const std::string& key, const blackwhitelist_t& blackwhitelist) const;
  bool _getEntry(const std::string& key, const blackwhitelist_t& blackwhitelist, BlackWhiteListEntry& ret_ble) const;
  bool _deleteEntry(const std::string& key, blackwhitelist_t& blackwhitelist);
  time_t _getExpiration(const std::string& key, const blackwhitelist_t& blackwhitelist) const; // returns number of seconds until expiration
  void _purgeEntries(BLWLType blt, blackwhitelist_t& blackwhitelist, BLWLType bl_type);
  void addEntryLog(BLWLType blt, const std::string& key, time_t seconds, const std::string& reason) const;
  void deleteEntryLog(BLWLType blt, const std::string& key) const;
  void expireEntryLog(BLWLType blt, const std::string& key) const;
  std::string ipLoginStr(const ComboAddress& ca, const std::string& login) const;
  bool checkSetupContext();
  bool addPersistEntry(const std::string& key, time_t seconds, BLWLType bl_type, const std::string& reason);
  bool deletePersistEntry(const std::string& key, BLWLType bl_type, blackwhitelist_t& blackwhitelist);
  BLWLType BLWLNameToType(const std::string& bl_name) const;
  std::string BLWLTypeToName(BLWLType bl_type) const;
};

extern BlackWhiteListDB g_bl_db;
extern BlackWhiteListDB g_wl_db;

// These are expected to be instantiated elsewhere
extern WebHookRunner g_webhook_runner;
extern WebHookDB g_webhook_db;
