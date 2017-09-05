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

enum BLType { IP_BL=0, LOGIN_BL=1, IP_LOGIN_BL=2, NONE_BL=999 };

class BlackListDB {
public:  
  BlackListDB() { redis_context = NULL; redis_port = 6379; redis_timeout=1; }
  BlackListDB(const BlackListDB&) = delete;

  void addEntry(const Netmask& nm, time_t seconds, const std::string& reason);
  void addEntry(const ComboAddress& ca, time_t seconds, const std::string& reason);
  void addEntry(const std::string& login, time_t seconds, const std::string& reason);
  void addEntry(const ComboAddress& ca, const std::string& login, time_t seconds, const std::string& reason);

  bool checkEntry(const ComboAddress& ca) const;
  bool checkEntry(const std::string& login) const;
  bool checkEntry(const ComboAddress& ca, const std::string& login) const;

  bool getEntry(const ComboAddress& ca, BlackListEntry& ret) const;
  bool getEntry(const std::string& login, BlackListEntry& ret) const;
  bool getEntry(const ComboAddress& ca, const std::string& login, BlackListEntry& ret) const;

  void deleteEntry(const Netmask& nm);
  void deleteEntry(const ComboAddress& ca);
  void deleteEntry(const std::string& login);
  void deleteEntry(const ComboAddress& ca, const std::string& login);

  time_t getExpiration(const ComboAddress& ca) const;
  time_t getExpiration(const std::string& login) const;
  time_t getExpiration(const ComboAddress& ca, const std::string& login) const;

  void addEntryInternal(const std::string& key, time_t seconds, BLType bl_type, const std::string& reason, bool replicate);
  void deleteEntryInternal(const std::string& key, BLType bl_type, bool replicate);

  void makePersistent(const std::string& host, unsigned int port);
  void persistReplicated() { persist_replicated = true; }
  bool loadPersistEntries();

  void purgeEntries();

  static void purgeEntriesThread(BlackListDB* bl_db)
  {
    bl_db->purgeEntries();
  }

  std::vector<BlackListEntry> getIPEntries() const;
  std::vector<BlackListEntry> getLoginEntries() const;
  std::vector<BlackListEntry> getIPLoginEntries() const;

  void setConnectTimeout(int timeout);
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

  const char* bl_names[4] = { "ip_bl", "login_bl", "ip_login_bl", NULL };
  const char* key_names[4] = { "ip", "login", "ip_login", NULL };
  NetmaskGroup ipbl_netmask;
  blacklist_t ip_blacklist;
  blacklist_t login_blacklist;
  blacklist_t ip_login_blacklist;
  mutable pthread_rwlock_t bl_rwlock = PTHREAD_RWLOCK_INITIALIZER;
  bool persist=false;
  bool persist_replicated=false;
  std::string redis_server;
  unsigned int redis_port;
  redisContext* redis_context;
  std::atomic<int> redis_timeout;
  
  void _addEntry(const std::string& key, time_t seconds, blacklist_t& blacklist, const std::string& reason);
  bool _checkEntry(const std::string& key, const blacklist_t& blacklist) const;
  bool _getEntry(const std::string& key, const blacklist_t& blacklist, BlackListEntry& ret_ble) const;
  bool _deleteEntry(const std::string& key, blacklist_t& blacklist);
  time_t _getExpiration(const std::string& key, const blacklist_t& blacklist) const; // returns number of seconds until expiration
  void _purgeEntries(BLType blt, blacklist_t& blacklist, BLType bl_type);
  void addEntryLog(BLType blt, const std::string& key, time_t seconds, const std::string& reason) const;
  void deleteEntryLog(BLType blt, const std::string& key) const;
  void expireEntryLog(BLType blt, const std::string& key) const;
  std::string ipLoginStr(const ComboAddress& ca, const std::string& login) const;
  bool checkSetupContext();
  bool addPersistEntry(const std::string& key, time_t seconds, BLType bl_type, const std::string& reason);
  bool deletePersistEntry(const std::string& key, BLType bl_type, blacklist_t& blacklist);
  BLType BLNameToType(const std::string& bl_name) const;
  std::string BLTypeToName(BLType bl_type) const;
};

extern BlackListDB g_bl_db;
