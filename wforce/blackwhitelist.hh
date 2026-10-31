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

struct redisSSLContext;

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

enum BLWLType { IP_BLWL=0, LOGIN_BLWL=1, IP_LOGIN_BLWL=2, JA3_BLWL=3, IP_JA3_BLWL=4, NONE_BLWL=999 };
enum class BLWLDBType { BLACKLIST=0, WHITELIST=1 };

class BlackWhiteListDB {
public:  
  BlackWhiteListDB(BLWLDBType type) {
    db_type = type;
    if (type == BLWLDBType::BLACKLIST) {
      list_names = { "ip_bl", "login_bl", "ip_login_bl", "ja3_bl", "ip_ja3_bl" };
      redis_prefix = "wfbl";
      ip_ret_msg = "Temporarily blacklisted IP Address - try again later";
      login_ret_msg = "Temporarily blacklisted Login Name - try again later";
      iplogin_ret_msg = "Temporarily blacklisted IP/Login Tuple - try again later";
      ja3_ret_msg = "Temporarily blacklisted JA3 Hash - try again later";
      ipja3_ret_msg = "Temporarily blacklisted IP/JA3 Hash Tuple - try again later";
    }
    else {
      list_names = { "ip_wl", "login_wl", "ip_login_wl", "ja3_wl", "ip_ja3_wl" };
      redis_prefix = "wfwl";
      ip_ret_msg = "Whitelisted IP Address";
      login_ret_msg = "Whitelisted Login Name";
      iplogin_ret_msg = "Whitelisted IP/Login Tuple";
      ja3_ret_msg = "Whitelisted JA3 Hash";
      ipja3_ret_msg = "Whitelisted IP/JA3 Hash";
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
  void addEntry(const ComboAddress& ca, const std::string& login, std::uint8_t prefix, time_t seconds, const std::string& reason);
  void addJA3Entry(const std::string& ja3, time_t seconds, const std::string& reason);
  void addIPJA3Entry(const ComboAddress& ca, const std::string& ja3, time_t seconds, const std::string& reason);
  void addIPJA3Entry(const ComboAddress& ca, const std::string& ja3, std::uint8_t prefix, time_t seconds, const std::string& reason);

  bool checkEntry(const ComboAddress& ca) const;
  bool checkEntry(const std::string& login) const;
  bool checkEntry(const ComboAddress& ca, const std::string& login) const;
  bool checkEntry(const ComboAddress& ca, const std::string& login, std::uint8_t prefix) const;
  bool checkJA3Entry(const std::string& ja3) const;
  bool checkIPJA3Entry(const ComboAddress& ca, const std::string& ja3) const;
  bool checkIPJA3Entry(const ComboAddress& ca, const std::string& ja3, std::uint8_t prefix) const;

  bool getEntry(const ComboAddress& ca, BlackWhiteListEntry& ret) const;
  bool getEntry(const std::string& login, BlackWhiteListEntry& ret) const;
  bool getEntry(const ComboAddress& ca, const std::string& login, BlackWhiteListEntry& ret) const;
  bool getEntry(const ComboAddress& ca, const std::string& login, std::uint8_t prefix, BlackWhiteListEntry& ret) const;
  bool getJA3Entry(const std::string& ja3, BlackWhiteListEntry& ret) const;
  bool getIPJA3Entry(const ComboAddress& ca, const std::string& ja3, BlackWhiteListEntry& ret) const;
  bool getIPJA3Entry(const ComboAddress& ca, const std::string& ja3, std::uint8_t prefix, BlackWhiteListEntry& ret) const;

  void deleteEntry(const Netmask& nm);
  void deleteEntry(const ComboAddress& ca);
  void deleteEntry(const std::string& login);
  void deleteEntry(const ComboAddress& ca, const std::string& login);
  void deleteEntry(const ComboAddress& ca, const std::string& login, std::uint8_t prefix);
  void deleteJA3Entry(const std::string& ja3);
  void deleteIPJA3Entry(const ComboAddress& ca, const std::string& ja3);
  void deleteIPJA3Entry(const ComboAddress& ca, const std::string& ja3, std::uint8_t prefix);

  time_t getExpiration(const ComboAddress& ca) const;
  time_t getExpiration(const std::string& login) const;
  time_t getExpiration(const ComboAddress& ca, const std::string& login) const;
  time_t getExpiration(const ComboAddress& ca, const std::string& login, std::uint8_t prefix) const;
  time_t getJA3Expiration(const std::string& ja3) const;
  time_t getIPJA3Expiration(const ComboAddress& ca, const std::string& ja3) const;
  time_t getIPJA3Expiration(const ComboAddress& ca, const std::string& ja3, std::uint8_t prefix) const;

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
  std::vector<BlackWhiteListEntry> getJA3Entries() const;
  std::vector<BlackWhiteListEntry> getIPJA3Entries() const;

  void setConnectTimeout(int timeout);
  void setRWTimeout(int timeout_secs, int timeout_usecs);
  void setv4Prefix(std::uint8_t bits);
  void setv6Prefix(std::uint8_t bits);
  std::uint8_t getv4Prefix() const { return v4_prefix.load(); }
  std::uint8_t getv6Prefix() const { return v6_prefix.load(); }

  const std::string& getIPRetMsg() const { return ip_ret_msg;}
  const std::string& getLoginRetMsg() const { return login_ret_msg; }
  const std::string& getIPLoginRetMsg() const { return iplogin_ret_msg; }
  const std::string& getJA3RetMsg() const { return ja3_ret_msg; }
  const std::string& getIPJA3RetMsg() const { return ipja3_ret_msg; }
  void setIPRetMsg(const std::string& msg) { ip_ret_msg = msg; }
  void setLoginRetMsg(const std::string& msg) { login_ret_msg = msg; }
  void setIPLoginRetMsg(const std::string& msg) { iplogin_ret_msg = msg; }
  void setJA3RetMsg(const std::string& msg) { ja3_ret_msg = msg; }
  void setIPJA3RetMsg(const std::string& msg) { ipja3_ret_msg = msg; }

  void setRedisUsername(const std::string& username) { redis_username = username; }
  void setRedisPassword(const std::string& password) { redis_password = password; }

  void setRedisTLS(bool enable);
  void setRedisTLSCA(const std::string& ca_file, const std::string& ca_dir) { redis_tls_ca_file = ca_file; redis_tls_ca_dir = ca_dir; }
  void setRedisTLSClientCert(const std::string& cert_file, const std::string& key_file) { redis_tls_cert_file = cert_file; redis_tls_key_file = key_file; }
  void setRedisTLSVerifyPeer(bool verify) { redis_tls_verify_peer = verify; }
  void setRedisTLSServerName(const std::string& server_name) { redis_tls_server_name = server_name; }

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

  std::vector<std::string> list_names = { "ip_blwl", "login_blwl", "ip_login_blwl", "ja3_blwl", "ip_ja3_blwl" };
  std::vector<std::string> key_names = { "ip", "login", "ip_login", "ja3", "ip_ja3"};
  NetmaskGroup iplist_netmask;
  blackwhitelist_t ip_list;
  blackwhitelist_t login_list;
  blackwhitelist_t ip_login_list;
  blackwhitelist_t ja3_list;
  blackwhitelist_t ip_ja3_list;
  mutable pthread_rwlock_t rwlock = PTHREAD_RWLOCK_INITIALIZER;
  bool persist=false;
  bool persist_replicated=false;
  std::string redis_server;
  unsigned int redis_port;
  std::string redis_password;
  std::string redis_username;
  bool redis_tls = false;
  bool redis_tls_verify_peer = true;
  std::string redis_tls_ca_file;
  std::string redis_tls_ca_dir;
  std::string redis_tls_cert_file;
  std::string redis_tls_key_file;
  std::string redis_tls_server_name;
  struct redisSSLContext* redis_ssl_context = nullptr;
  redisContext* redis_context;
  std::atomic<int> redis_timeout;
  std::atomic<int> redis_rw_timeout_secs;
  std::atomic<int> redis_rw_timeout_usecs;
  std::string redis_prefix = {"wfbl"};
  BLWLDBType db_type;
  std::string ip_ret_msg;
  std::string login_ret_msg;
  std::string iplogin_ret_msg;
  std::string ipja3_ret_msg;
  std::string ja3_ret_msg;
  std::atomic<std::uint8_t> v4_prefix{32};
  std::atomic<std::uint8_t> v6_prefix{128};

  std::vector<BlackWhiteListEntry> getEntries(const blackwhitelist_t& list) const;
  void _addEntry(const std::string& key, time_t seconds, blackwhitelist_t& blackwhitelist, const std::string& reason);
  bool _checkEntry(const std::string& key, const blackwhitelist_t& blackwhitelist) const;
  bool _getEntry(const std::string& key, const blackwhitelist_t& blackwhitelist, BlackWhiteListEntry& ret_ble) const;
  bool _deleteEntry(const std::string& key, blackwhitelist_t& blackwhitelist);
  time_t _getExpiration(const std::string& key, const blackwhitelist_t& blackwhitelist) const; // returns number of seconds until expiration
  void _purgeEntries(blackwhitelist_t& blackwhitelist, BLWLType bl_type);
  void addEntryLog(BLWLType blt, const std::string& key, time_t seconds, const std::string& reason) const;
  void deleteEntryLog(BLWLType blt, const std::string& key) const;
  void expireEntryLog(BLWLType blt, const std::string& key) const;
  std::uint8_t getDefaultPrefix(const ComboAddress& ca) const;
  std::string ipString(const ComboAddress& ca) const;
  std::string ipStringStr(const ComboAddress& ca, const std::string& login) const;
  std::string ipStringStr(const ComboAddress& ca, std::uint8_t prefix, const std::string& str) const;
  bool checkSetupContext();
  redisContext* connectRedis(const std::string& host, unsigned int port, struct redisSSLContext*& ssl_context);
  bool authRedis(redisContext* context, const std::string& username, const std::string& password, const std::string& desc);
  void resetContext();
  void incConnFailed();
  struct redisSSLContext* createTLSContext(const std::string& host);
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
