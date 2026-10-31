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
#include "blackwhitelist.hh"
#include "replication.hh"
#include "replication_bl.hh"
#include "replication_wl.hh"
#include "replication.pb.h"
#include "wforce-prometheus.hh"
#include "json11.hpp"
#include <boost/version.hpp>
#include <boost/date_time/posix_time/ptime.hpp>
#include <boost/date_time/gregorian/gregorian.hpp>
#include <boost/date_time/posix_time/posix_time_duration.hpp>
#include <hiredis/hiredis.h>
#ifdef HAVE_LIBHIREDIS_SSL
#include <hiredis/hiredis_ssl.h>
#endif
#include <ostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include "iputils.hh"

using namespace json11;

BlackWhiteListDB g_bl_db(BLWLDBType::BLACKLIST);
BlackWhiteListDB g_wl_db(BLWLDBType::WHITELIST);

std::uint8_t BlackWhiteListDB::getDefaultPrefix(const ComboAddress& ca) const
{
  if (ca.isIpv4()) {
    return v4_prefix.load();
  }
  return v6_prefix.load();
}

std::string BlackWhiteListDB::ipString(const ComboAddress& ca) const
{
  return Netmask(ca, getDefaultPrefix(ca)).toStringNetwork();
}

std::string BlackWhiteListDB::ipStringStr(const ComboAddress& ca, const std::string& str) const
{
  return ipString(ca) + ":" + str;
}

std::string BlackWhiteListDB::ipStringStr(const ComboAddress& ca, std::uint8_t prefix, const std::string& str) const
{
  return Netmask(ca, prefix).toStringNetwork() + ":" + str;
}

void BlackWhiteListDB::addEntry(const Netmask& nm, time_t seconds, const std::string& reason)
{
  std::string key = nm.toStringNetwork();

  addEntryInternal(key, seconds, IP_BLWL, reason, true);
  addEntryLog(IP_BLWL, key, seconds, reason);
}

void BlackWhiteListDB::addEntry(const ComboAddress& ca, time_t seconds, const std::string& reason)
{
  std::string key = ipString(ca);

  addEntryInternal(key, seconds, IP_BLWL, reason, true);
  addEntryLog(IP_BLWL, key, seconds, reason);
}

void BlackWhiteListDB::addEntry(const std::string& login, time_t seconds, const std::string& reason)
{
  addEntryInternal(login, seconds, LOGIN_BLWL, reason, true);
  addEntryLog(LOGIN_BLWL, login, seconds, reason);
}

void
BlackWhiteListDB::addEntry(const ComboAddress& ca, const std::string& login, time_t seconds, const std::string& reason)
{
  std::string key = ipStringStr(ca, login);
  addEntryInternal(key, seconds, IP_LOGIN_BLWL, reason, true);
  addEntryLog(IP_LOGIN_BLWL, key, seconds, reason);
}

void
BlackWhiteListDB::addEntry(const ComboAddress& ca, const std::string& login, std::uint8_t prefix, time_t seconds, const std::string& reason)
{
  std::string key = ipStringStr(ca, prefix, login);
  addEntryInternal(key, seconds, IP_LOGIN_BLWL, reason, true);
  addEntryLog(IP_LOGIN_BLWL, key, seconds, reason);
}

void BlackWhiteListDB::addJA3Entry(const std::string& ja3, time_t seconds, const std::string& reason)
{
  addEntryInternal(ja3, seconds, JA3_BLWL, reason, true);
  addEntryLog(JA3_BLWL, ja3, seconds, reason);
}

void
BlackWhiteListDB::addIPJA3Entry(const ComboAddress& ca, const std::string& ja3, time_t seconds, const std::string& reason)
{
  std::string key = ipStringStr(ca, ja3);
  addEntryInternal(key, seconds, IP_JA3_BLWL, reason, true);
  addEntryLog(IP_JA3_BLWL, key, seconds, reason);
}

void
BlackWhiteListDB::addIPJA3Entry(const ComboAddress& ca, const std::string& ja3,  std::uint8_t prefix, time_t seconds, const std::string& reason)
{
  std::string key = ipStringStr(ca, prefix, ja3);
  addEntryInternal(key, seconds, IP_JA3_BLWL, reason, true);
  addEntryLog(IP_JA3_BLWL, key, seconds, reason);
}

void BlackWhiteListDB::addEntryInternal(const std::string& key, time_t seconds, BLWLType blwl_type,
                                        const std::string& reason, bool replicate)
{
  WriteLock wl(&rwlock);

  auto add = [&key, replicate, &reason, seconds, blwl_type, this] {
    addEntryLog(blwl_type, key, seconds, reason);
    if (persist && ((replicate == true) || ((replicate == false) && persist_replicated)))
      addPersistEntry(key, seconds, blwl_type, reason);
    if (replicate == true) {
      if (db_type == BLWLDBType::BLACKLIST) {
        std::shared_ptr<BLReplicationOperation> bl_rop;
        bl_rop = std::make_shared<BLReplicationOperation>(BLOperation_BLOpType_BLAdd, blwl_type, key, seconds, reason);
        ReplicationOperation rep_op(bl_rop, WforceReplicationMsg_RepType_BlacklistType);
        replicateOperation(rep_op);
      }
      else {
        std::shared_ptr<WLReplicationOperation> wl_rop;
        wl_rop = std::make_shared<WLReplicationOperation>(BLOperation_BLOpType_BLAdd, blwl_type, key, seconds, reason);
        ReplicationOperation rep_op(wl_rop, WforceReplicationMsg_RepType_WhitelistType);
        replicateOperation(rep_op);
      }
    }
  };
  switch (blwl_type) {
    case IP_BLWL:
      _addEntry(key, seconds, ip_list, reason);
      iplist_netmask.addMask(key);
      add();
      break;
    case LOGIN_BLWL:
      _addEntry(key, seconds, login_list, reason);
      add();
      break;
    case IP_LOGIN_BLWL:
      _addEntry(key, seconds, ip_login_list, reason);
      add();
      break;
    case JA3_BLWL:
      _addEntry(key, seconds, ja3_list, reason);
      add();
      break;
    case IP_JA3_BLWL:
      _addEntry(key, seconds, ip_ja3_list, reason);
      add();
      break;
    default:
      break;
  }
  std::string event_name;
  std::string type_name;
  if (db_type == BLWLDBType::BLACKLIST) {
    event_name = "addbl";
    type_name = "bl_type";
  }
  else {
    event_name = "addwl";
    type_name = "wl_type";
  }
  // only generate webhook for this event for the first add, not for replicas
  if (replicate == true) {
    Json jobj = Json::object{{"key",         key},
                             {type_name,     BLWLTypeToName(blwl_type)},
                             {"reason",      reason},
                             {"expire_secs", (int) seconds},
                             {"type",        "wforce_addblwl"}};
    for (const auto& h: g_webhook_db.getWebHooksForEvent(event_name)) {
      if (auto hs = h.lock())
        g_webhook_runner.runHook(event_name, hs, jobj);
    }
  }
}

void BlackWhiteListDB::_addEntry(const std::string& key, time_t seconds, blackwhitelist_t& blackwhitelist,
                                 const std::string& reason)
{
  BlackWhiteListEntry bl;

  bl.key = key;
  bl.expiration = boost::posix_time::from_time_t(time(NULL)) + boost::posix_time::seconds(seconds);
  bl.reason = reason;

  auto& keyindex = blackwhitelist.get<KeyTag>();
  keyindex.erase(key);

  blackwhitelist.insert(bl);
}

bool BlackWhiteListDB::checkEntry(const ComboAddress& ca) const
{
  ReadLock wl(&rwlock);
  return iplist_netmask.match(ca);
}

bool BlackWhiteListDB::checkEntry(const std::string& login) const
{
  return _checkEntry(login, login_list);
}

bool BlackWhiteListDB::checkEntry(const ComboAddress& ca, const std::string& login) const
{
  return _checkEntry(ipStringStr(ca, login), ip_login_list);
}

bool BlackWhiteListDB::checkEntry(const ComboAddress& ca, const std::string& login, std::uint8_t prefix) const
{
  return _checkEntry(ipStringStr(ca, prefix, login), ip_login_list);
}

bool BlackWhiteListDB::checkJA3Entry(const std::string& ja3) const
{
  return _checkEntry(ja3, ja3_list);
}

bool BlackWhiteListDB::checkIPJA3Entry(const ComboAddress& ca, const std::string& ja3) const
{
  return _checkEntry(ipStringStr(ca, ja3), ip_ja3_list);
}

bool BlackWhiteListDB::checkIPJA3Entry(const ComboAddress& ca, const std::string& ja3, std::uint8_t prefix) const
{
  return _checkEntry(ipStringStr(ca, prefix, ja3), ip_ja3_list);
}

bool BlackWhiteListDB::_checkEntry(const std::string& key, const blackwhitelist_t& blackwhitelist) const
{
  ReadLock rl(&rwlock);

  auto& keyindex = blackwhitelist.get<KeyTag>();
  auto kit = keyindex.find(key);

  if (kit != keyindex.end())
    return true;
  return false;
}

bool BlackWhiteListDB::getEntry(const ComboAddress& ca, BlackWhiteListEntry& ret) const
{
  Netmask nm;
  {
    ReadLock wl(&rwlock);
    iplist_netmask.lookup(ca, &nm);
  }
  return _getEntry(nm.toString(), ip_list, ret);
}

bool BlackWhiteListDB::getEntry(const std::string& login, BlackWhiteListEntry& ret) const
{
  return _getEntry(login, login_list, ret);
}

bool BlackWhiteListDB::getEntry(const ComboAddress& ca, const std::string& login, BlackWhiteListEntry& ret) const
{
  return _getEntry(ipStringStr(ca, login), ip_login_list, ret);
}

bool BlackWhiteListDB::getEntry(const ComboAddress& ca, const std::string& login, std::uint8_t prefix, BlackWhiteListEntry& ret) const
{
  return _getEntry(ipStringStr(ca, prefix, login), ip_login_list, ret);
}

bool BlackWhiteListDB::getJA3Entry(const std::string& ja3, BlackWhiteListEntry& ret) const
{
  return _getEntry(ja3, ja3_list, ret);
}

bool BlackWhiteListDB::getIPJA3Entry(const ComboAddress& ca, const std::string& ja3, BlackWhiteListEntry& ret) const
{
  return _getEntry(ipStringStr(ca, ja3), ip_ja3_list, ret);
}

bool BlackWhiteListDB::getIPJA3Entry(const ComboAddress& ca, const std::string& ja3, std::uint8_t prefix, BlackWhiteListEntry& ret) const
{
  return _getEntry(ipStringStr(ca, prefix, ja3), ip_ja3_list, ret);
}

bool BlackWhiteListDB::_getEntry(const std::string& key, const blackwhitelist_t& blackwhitelist,
                                 BlackWhiteListEntry& ret_ble) const
{
  ReadLock rl(&rwlock);

  auto& keyindex = blackwhitelist.get<KeyTag>();
  auto kit = keyindex.find(key);

  if (kit != keyindex.end()) {
    ret_ble = *kit; // copy
    return true;
  }
  return false;
}

void BlackWhiteListDB::deleteEntry(const Netmask& nm)
{
  std::string key = nm.toStringNetwork();

  deleteEntryInternal(key, IP_BLWL, true);
}

void BlackWhiteListDB::deleteEntry(const ComboAddress& ca)
{
  std::string key = ipString(ca);

  deleteEntryInternal(key, IP_BLWL, true);
}

void BlackWhiteListDB::deleteEntry(const std::string& login)
{
  deleteEntryInternal(login, LOGIN_BLWL, true);
}

void BlackWhiteListDB::deleteEntry(const ComboAddress& ca, const std::string& login)
{
  std::string key = ipStringStr(ca, login);

  deleteEntryInternal(key, IP_LOGIN_BLWL, true);
}

void BlackWhiteListDB::deleteEntry(const ComboAddress& ca, const std::string& login, std::uint8_t prefix)
{
  std::string key = ipStringStr(ca, prefix, login);

  deleteEntryInternal(key, IP_LOGIN_BLWL, true);
}

void BlackWhiteListDB::deleteJA3Entry(const std::string& ja3)
{
  deleteEntryInternal(ja3, JA3_BLWL, true);
}

void BlackWhiteListDB::deleteIPJA3Entry(const ComboAddress& ca, const std::string& ja3)
{
  std::string key = ipStringStr(ca, ja3);

  deleteEntryInternal(key, IP_JA3_BLWL, true);
}

void BlackWhiteListDB::deleteIPJA3Entry(const ComboAddress& ca, const std::string& ja3, std::uint8_t prefix)
{
  std::string key = ipStringStr(ca, prefix, ja3);

  deleteEntryInternal(key, IP_JA3_BLWL, true);
}

void BlackWhiteListDB::deleteEntryInternal(const std::string& key, BLWLType blwl_type, bool replicate)
{
  WriteLock wl(&rwlock);

  auto del = [this, &key, blwl_type, replicate] {
    deleteEntryLog(blwl_type, key);
    if (persist && ((replicate == true) || ((replicate == false) && persist_replicated))) {
      deletePersistEntry(key, blwl_type, ip_list);
    }
    if (replicate == true) {
      if (db_type == BLWLDBType::BLACKLIST) {
        std::shared_ptr<BLReplicationOperation> bl_rop;
        bl_rop = std::make_shared<BLReplicationOperation>(BLOperation_BLOpType_BLDelete, blwl_type, key, 0, "");
        ReplicationOperation rep_op(bl_rop, WforceReplicationMsg_RepType_BlacklistType);
        replicateOperation(rep_op);
      }
      else {
        std::shared_ptr<WLReplicationOperation> wl_rop;
        wl_rop = std::make_shared<WLReplicationOperation>(BLOperation_BLOpType_BLDelete, blwl_type, key, 0, "");
        ReplicationOperation rep_op(wl_rop, WforceReplicationMsg_RepType_WhitelistType);
        replicateOperation(rep_op);
      }
    }
  };

  switch (blwl_type) {
    case IP_BLWL:
      _deleteEntry(key, ip_list);
      iplist_netmask.deleteMask(key);
      del();
      break;
    case LOGIN_BLWL:
      _deleteEntry(key, login_list);
      del();
      break;
    case IP_LOGIN_BLWL:
      _deleteEntry(key, ip_login_list);
      del();
      break;
    case JA3_BLWL:
      _deleteEntry(key, ja3_list);
      del();
      break;
    case IP_JA3_BLWL:
      _deleteEntry(key, ip_ja3_list);
      del();
      break;
    default:
      break;
  }
  std::string event_name;
  std::string type_name;
  if (db_type == BLWLDBType::BLACKLIST) {
    event_name = "delbl";
    type_name = "bl_type";
  }
  else {
    event_name = "delwl";
    type_name = "wl_type";
  }
  // only generate webhook for this event for the first delete, not for replicas
  if (replicate == true) {
    Json jobj = Json::object{{"key",     key},
                             {type_name, BLWLTypeToName(blwl_type)},
                             {"type",    "wforce_delblwl"}};
    for (const auto& h: g_webhook_db.getWebHooksForEvent(event_name)) {
      if (auto hs = h.lock())
        g_webhook_runner.runHook(event_name, hs, jobj);
    }
  }
}

bool BlackWhiteListDB::_deleteEntry(const std::string& key, blackwhitelist_t& blackwhitelist)
{
  auto& keyindex = blackwhitelist.get<KeyTag>();
  auto kit = keyindex.find(key);

  if (kit != keyindex.end()) {
    keyindex.erase(key);
    return true;
  }
  return false;
}

time_t BlackWhiteListDB::getExpiration(const ComboAddress& ca) const
{
  Netmask nm;
  iplist_netmask.lookup(ca, &nm);
  return _getExpiration(nm.toString(), ip_list);
}

time_t BlackWhiteListDB::getExpiration(const std::string& login) const
{
  return _getExpiration(login, login_list);
}

time_t BlackWhiteListDB::getExpiration(const ComboAddress& ca, const std::string& login) const
{
  return _getExpiration(ipStringStr(ca, login), ip_login_list);
}

time_t BlackWhiteListDB::getExpiration(const ComboAddress& ca, const std::string& login, std::uint8_t prefix) const
{
  return _getExpiration(ipStringStr(ca, prefix, login), ip_login_list);
}

time_t BlackWhiteListDB::getJA3Expiration(const std::string& ja3) const
{
  return _getExpiration(ja3, ja3_list);
}

time_t BlackWhiteListDB::getIPJA3Expiration(const ComboAddress& ca, const std::string& ja3) const
{
  return _getExpiration(ipStringStr(ca, ja3), ip_ja3_list);
}

time_t BlackWhiteListDB::getIPJA3Expiration(const ComboAddress& ca, const std::string& ja3, std::uint8_t prefix) const
{
  return _getExpiration(ipStringStr(ca, prefix, ja3), ip_ja3_list);
}

// to_time_t is missing in some versions of boost
#if BOOST_VERSION < 105700
inline time_t my_to_time_t(boost::posix_time::ptime pt)
{
  boost::posix_time::time_duration dur = pt - boost::posix_time::ptime(boost::gregorian::date(1970,1,1));
  return std::time_t(dur.total_seconds());
}
#define TO_TIME_T my_to_time_t
#else
#define TO_TIME_T boost::posix_time::to_time_t
#endif

time_t BlackWhiteListDB::_getExpiration(const std::string& key, const blackwhitelist_t& blackwhitelist) const
{
  ReadLock rl(&rwlock);

  auto& keyindex = blackwhitelist.get<KeyTag>();
  auto kit = keyindex.find(key);

  if (kit != keyindex.end()) {
    time_t expiration_secs = TO_TIME_T(kit->expiration) - time(NULL);
    if (expiration_secs >= 0)
      return expiration_secs;
    else
      return -1; // expired already
  }
  return (time_t) -1; // not found
}

void BlackWhiteListDB::purgeEntries()
{
  while (true) {
    sleep(1);
    _purgeEntries(ip_list, IP_BLWL);
    _purgeEntries(login_list, LOGIN_BLWL);
    _purgeEntries(ip_login_list, IP_LOGIN_BLWL);
    _purgeEntries(ja3_list, JA3_BLWL);
    _purgeEntries(ip_ja3_list, IP_JA3_BLWL);
    {
      ReadLock rl(&rwlock);
      if (db_type == BLWLDBType::BLACKLIST) {
        setPrometheusBLIPEntries(ip_list.size());
        setPrometheusBLLoginEntries(login_list.size());
        setPrometheusBLIPLoginEntries(ip_login_list.size());
        setPrometheusBLJA3Entries(ja3_list.size());
        setPrometheusBLIPJA3Entries(ip_ja3_list.size());
      }
      else {
        setPrometheusWLIPEntries(ip_list.size());
        setPrometheusWLLoginEntries(login_list.size());
        setPrometheusWLIPLoginEntries(ip_login_list.size());
        setPrometheusWLJA3Entries(ja3_list.size());
        setPrometheusWLIPJA3Entries(ip_ja3_list.size());
      }
    }
  }
}

void BlackWhiteListDB::_purgeEntries(blackwhitelist_t& blackwhitelist, BLWLType blwl_type)
{
  WriteLock wl(&rwlock);
  boost::system_time now = boost::get_system_time();

  auto& timeindex = blackwhitelist.get<TimeTag>();

  std::string event_name;
  std::string type_name;
  if (db_type == BLWLDBType::BLACKLIST) {
    event_name = "expirebl";
    type_name = "bl_type";
  }
  else {
    event_name = "expirewl";
    type_name = "wl_type";
  }
  for (auto tit = timeindex.begin(); tit != timeindex.end();) {
    if (tit->expiration <= now) {
      Json jobj = Json::object{{"key",     tit->key},
                               {type_name, BLWLTypeToName(blwl_type)},
                               {"type",    "wforce_expireblwl"}};
      for (const auto& h: g_webhook_db.getWebHooksForEvent(event_name)) {
        if (auto hs = h.lock())
          g_webhook_runner.runHook(event_name, hs, jobj);
      }
      expireEntryLog(blwl_type, tit->key);
      if (blwl_type == IP_BLWL)
        iplist_netmask.deleteMask(tit->key);
      tit = timeindex.erase(tit);
    }
    else
      break; // stop when we run out of entries to expire
  }
}

std::vector<BlackWhiteListEntry> BlackWhiteListDB::getEntries(const blackwhitelist_t& list) const
{
  ReadLock rl(&rwlock);
  std::vector<BlackWhiteListEntry> ret;

  auto& seqindex = list.get<SeqTag>();
  for (const auto& a: seqindex)
    ret.push_back(a);
  return ret;
}

std::vector<BlackWhiteListEntry> BlackWhiteListDB::getIPEntries() const
{
  return getEntries(ip_list);
}

std::vector<BlackWhiteListEntry> BlackWhiteListDB::getLoginEntries() const
{
  return getEntries(login_list);
}

std::vector<BlackWhiteListEntry> BlackWhiteListDB::getIPLoginEntries() const
{
  return getEntries(ip_login_list);
}

std::vector<BlackWhiteListEntry> BlackWhiteListDB::getJA3Entries() const
{
  return getEntries(ja3_list);
}

std::vector<BlackWhiteListEntry> BlackWhiteListDB::getIPJA3Entries() const
{
  return getEntries(ip_ja3_list);
}

void
BlackWhiteListDB::addEntryLog(BLWLType blt, const std::string& key, time_t seconds, const std::string& reason) const
{
  std::ostringstream os;
  std::string blwl_name = list_names[blt];
  std::string key_name = BLWLTypeToName(blt);
  std::string event_name;

  if (db_type == BLWLDBType::BLACKLIST)
    event_name = "addBLEntry";
  else
    event_name = "addWLEntry";

  os << event_name << " " << blwl_name << ": " << key_name << "=" << key << " expire_secs=" << std::to_string(seconds)
     << " reason=\"" << reason + "\"";
  noticelog(os.str().c_str());
}

void BlackWhiteListDB::deleteEntryLog(BLWLType blt, const std::string& key) const
{
  std::ostringstream os;
  std::string blwl_name = list_names[blt];
  std::string key_name = BLWLTypeToName(blt);
  std::string event_name;

  if (db_type == BLWLDBType::BLACKLIST)
    event_name = "deleteBLEntry";
  else
    event_name = "deleteWLEntry";

  os << event_name << " " << blwl_name << ": " << key_name << "=" + key;
  noticelog(os.str().c_str());
}

void BlackWhiteListDB::expireEntryLog(BLWLType blt, const std::string& key) const
{
  std::ostringstream os;
  std::string blwl_name = list_names[blt];
  std::string key_name = BLWLTypeToName(blt);
  std::string event_name;

  if (db_type == BLWLDBType::BLACKLIST)
    event_name = "expireBLEntry";
  else
    event_name = "expireWLEntry";

  os << event_name << " " << blwl_name << ": " << key_name << "=" << key;
  noticelog(os.str().c_str());
}

void BlackWhiteListDB::setConnectTimeout(int timeout)
{
  redis_timeout = timeout; // atomic
}

void BlackWhiteListDB::setRWTimeout(int timeout_secs, int timeout_usecs)
{
  redis_rw_timeout_usecs = timeout_usecs; // atomic
  redis_rw_timeout_secs = timeout_secs; // atomic
}

void BlackWhiteListDB::setv4Prefix(std::uint8_t bits)
{
  v4_prefix = bits > 32 ? 32 : bits;
}

void BlackWhiteListDB::setv6Prefix(std::uint8_t bits)
{
  v6_prefix = bits > 128 ? 128 : bits;
}

void BlackWhiteListDB::setRedisTLS(bool enable)
{
#ifndef HAVE_LIBHIREDIS_SSL
  if (enable) {
    errlog("setRedisTLS: wforce was built without Redis TLS support (hiredis_ssl)");
    throw WforceException("Error: wforce was built without Redis TLS support (hiredis_ssl)");
  }
#endif
  redis_tls = enable;
}

#ifdef HAVE_LIBHIREDIS_SSL
namespace {
  std::once_flag redis_ssl_init_flag;
}
#endif

namespace {
  void freeSSLContext(struct redisSSLContext*& ssl_context)
  {
#ifdef HAVE_LIBHIREDIS_SSL
    if (ssl_context != nullptr) {
      redisFreeSSLContext(ssl_context);
      ssl_context = nullptr;
    }
#endif
  }
}

void BlackWhiteListDB::incConnFailed()
{
  if (db_type == BLWLDBType::BLACKLIST)
    incPrometheusRedisBLConnFailed();
  else
    incPrometheusRedisWLConnFailed();
}

#ifdef HAVE_LIBHIREDIS_SSL
// The TLS context is recreated for every new connection so that config changes are picked up on reconnect,
// like the username/password used for AUTH
struct redisSSLContext* BlackWhiteListDB::createTLSContext(const std::string& host)
{
  std::call_once(redis_ssl_init_flag, []() { redisInitOpenSSL(); });

  redisSSLContextError ssl_error = REDIS_SSL_CTX_NONE;
  const char* ca_file = redis_tls_ca_file.empty() ? nullptr : redis_tls_ca_file.c_str();
  const char* ca_dir = redis_tls_ca_dir.empty() ? nullptr : redis_tls_ca_dir.c_str();
  const char* cert_file = redis_tls_cert_file.empty() ? nullptr : redis_tls_cert_file.c_str();
  const char* key_file = redis_tls_key_file.empty() ? nullptr : redis_tls_key_file.c_str();
  // We connect to the resolved IP address, so default the TLS server name (sent
  // via SNI) to the configured server name, unless that is itself an IP
  // address literal. Note that hiredis does not verify the certificate against
  // the server name - only the certificate chain is verified
  std::string sni_name = redis_tls_server_name;
  if (sni_name.empty()) {
    try {
      ComboAddress ca(host);
    }
    catch (const PDNSException& e) {
      sni_name = host;
    }
  }
  const char* server_name = sni_name.empty() ? nullptr : sni_name.c_str();
  redisSSLOptions ssl_options = {ca_file, ca_dir, cert_file, key_file, server_name,
                                 redis_tls_verify_peer ? REDIS_SSL_VERIFY_PEER : REDIS_SSL_VERIFY_NONE};
  struct redisSSLContext* ssl_context = redisCreateSSLContextWithOptions(&ssl_options, &ssl_error);
  if (ssl_context == nullptr || ssl_error != REDIS_SSL_CTX_NONE) {
    errlog("createTLSContext: could not create Redis TLS context: %s", redisSSLContextGetError(ssl_error));
    freeSSLContext(ssl_context);
    return nullptr;
  }
  return ssl_context;
}
#else
struct redisSSLContext* BlackWhiteListDB::createTLSContext(const std::string& host)
{
  return nullptr;
}
#endif

// Connects to host:port, trying all resolved addresses until one succeeds. If TLS is enabled, the TLS
// context is returned in ssl_context, and must be freed by the caller after the connection is freed
redisContext* BlackWhiteListDB::connectRedis(const std::string& host, unsigned int port, struct redisSSLContext*& ssl_context)
{
  redisContext* context = NULL;
  struct timeval tv;
  tv.tv_sec = redis_timeout;
  tv.tv_usec = 0;

  struct addrinfo hints;
  struct addrinfo *result, *rp;
  int s;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;    /* Allow IPv4 or IPv6 */

  s = getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hints, &result);
  if (s != 0) {
    errlog("connectRedis: getaddrinfo(%s): %s\n", host, gai_strerror(s));
    return NULL;
  }

  // Iterate through the returned IP addresses trying to connect to the all until we succeed
  for (rp = result; rp != NULL; rp = rp->ai_next) {
    std::string ip_addr = ComboAddress(rp->ai_addr, rp->ai_addrlen).toString();
    context = redisConnectWithTimeout(ip_addr.c_str(), port, tv);
    if (context == NULL || context->err) {
      infolog("connectRedis: could not connect to redis on (%s:%d), will try other addresses", ip_addr, port);
      if (context) {
        redisFree(context);
        context = NULL;
      }
    }
    else {
      break;
    }
  }
  freeaddrinfo(result);

  if (context == NULL) {
    return NULL;
  }

  tv.tv_sec = static_cast<time_t>(redis_rw_timeout_secs);
  tv.tv_usec = static_cast<suseconds_t>(redis_rw_timeout_usecs);
  if (redisSetTimeout(context, tv) == REDIS_ERR) {
    errlog("Could not set Redis Timeout to %u seconds, %u microseconds", tv.tv_sec, tv.tv_usec);
    redisFree(context);
    throw WforceException("Error: could not set Redis Timeout in BlackWhitelist");
  }
  else {
    infolog("Set Redis RW Timeout to %u seconds, %u microseconds", tv.tv_sec, tv.tv_usec);
  }
  if (redisEnableKeepAlive(context) == REDIS_ERR) {
    errlog("Could not enable Redis KeepAlive");
    redisFree(context);
    throw WforceException("Error: Could not enable Redis KeepAlive in BlackWhitelist");
  }
#ifdef HAVE_LIBHIREDIS_SSL
  if (redis_tls) {
    freeSSLContext(ssl_context);
    ssl_context = createTLSContext(host);
    if (ssl_context == nullptr) {
      redisFree(context);
      return NULL;
    }
    if (redisInitiateSSLWithContext(context, ssl_context) != REDIS_OK) {
      errlog("connectRedis: could not initiate TLS to redis (%s:%d): %s", host, port, context->errstr);
      redisFree(context);
      freeSSLContext(ssl_context);
      return NULL;
    }
    infolog("Established TLS connection to redis (%s:%d)", host, port);
  }
#endif
  return context;
}

// Returns false if the server rejected the credentials. An I/O error is not reported here, it
// is detected by the next command on the connection
bool BlackWhiteListDB::authRedis(redisContext* context, const std::string& username, const std::string& password, const std::string& desc)
{
  bool retval = true;
  redisReply* reply;
  if (username.length()) {
    reply = (redisReply*) redisCommand(context, "AUTH %s %s", username.c_str(), password.c_str());
  }
  else {
    reply = (redisReply*) redisCommand(context, "AUTH %s", password.c_str());
  }
  if (reply != NULL) {
    if (reply->type == REDIS_REPLY_ERROR) {
      if (username.length())
        errlog("checkSetupContext: Error in AUTH to %s (username: %s): %s", desc, username, reply->str);
      else
        errlog("checkSetupContext: Error in AUTH to %s: %s", desc, reply->str);
      retval = false;
    }
    freeReplyObject(reply);
  }
  return retval;
}

void BlackWhiteListDB::resetContext()
{
  if (redis_context) {
    redisFree(redis_context);
    redis_context = NULL;
  }
  freeSSLContext(redis_ssl_context);
}

bool BlackWhiteListDB::checkSetupContext()
{
  if (redis_context == NULL || redis_context->err) {
    // something went wrong previously, try again
    resetContext();
    redis_context = connectRedis(redis_server, redis_port, redis_ssl_context);
    if (redis_context == NULL) {
      errlog("checkSetupContext: could not connect to any addresses for redis BlackWhiteListDB (%s:%d)", redis_server, redis_port);
      incConnFailed();
      return false;
    }
  }

  // Now we have a working Redis connection, check if we need to authenticate
  if (redis_password.length()) {
    if (!authRedis(redis_context, redis_username, redis_password, "persistent redis BlackWhiteListDB")) {
      throw WforceException("Error: Error in AUTH to persistent redis BlackWhiteListDB");
    }
  }

  return true;
}

void BlackWhiteListDB::makePersistent(const std::string& host, unsigned int port = 6379)
{
  persist = true;
  redis_server = host;
  redis_port = port;
}

bool
BlackWhiteListDB::addPersistEntry(const std::string& key, time_t seconds, BLWLType blwl_type, const std::string& reason)
{
  bool retval = false;
  if (checkSetupContext()) {
    std::stringstream redis_key_s;
    std::stringstream redis_value_s;
    std::string blwl_key = BLWLTypeToName(blwl_type);
    time_t expiration_time = time(NULL) + seconds;

    redis_key_s << redis_prefix << ":" << blwl_key << ":" << key;
    redis_value_s << expiration_time << ":" << reason;
    redisReply* reply = (redisReply*) redisCommand(redis_context, "SET %s %b EX %d", redis_key_s.str().c_str(),
                                                   redis_value_s.str().c_str(), redis_value_s.str().length(), seconds);
    if (reply != NULL) {
      if (reply->type == REDIS_REPLY_ERROR) {
        errlog("addPersistEntry: Error adding %s to persistent redis BlackWhiteListDB (%s)", key, reply->str);
      }
      else
        retval = true;
      freeReplyObject(reply);
    }
  }
  if (db_type == BLWLDBType::BLACKLIST)
    incPrometheusRedisBLUpdates();
  else
    incPrometheusRedisWLUpdates();
  return retval;
}

bool BlackWhiteListDB::deletePersistEntry(const std::string& key, BLWLType blwl_type, blackwhitelist_t& blackwhitelist)
{
  bool retval = false;
  if (checkSetupContext()) {
    BlackWhiteListEntry ble;
    std::stringstream redis_key_s;
    std::string blwl_key = BLWLTypeToName(blwl_type);

    redis_key_s << redis_prefix << ":" << blwl_key << ":" << key;
    redisReply* reply = (redisReply*) redisCommand(redis_context, "DEL %s", redis_key_s.str().c_str());
    if (reply != NULL) {
      if (reply->type == REDIS_REPLY_ERROR) {
        errlog("deletePersistEntry: Error deleting %s from persistent redis BlackWhiteListDB (%s)", key, reply->str);
      }
      else
        retval = true;
      freeReplyObject(reply);
    }
  }
  return retval;
}

bool BlackWhiteListDB::loadPersistEntries()
{
  bool retval = true;
  WriteLock wl(&rwlock);

  if (persist != false) {
    unsigned int num_entries = 0;
    if (checkSetupContext()) {
      bool end_it = false;
      unsigned int count_it = 0;

      while (end_it != true) {
        std::ostringstream oss;
        oss << "SCAN %d MATCH " << redis_prefix << ":* COUNT 1000";
        redisReply* reply = (redisReply*) redisCommand(redis_context, oss.str().c_str(), count_it);
        if (reply != NULL) {
          if (reply->type == REDIS_REPLY_ERROR) {
            errlog("loadPersistEntries: Error scanning keys in persistent redis BlackWhiteListDB (%s)", reply->str);
            retval = false;
          }
          else if (reply->type == REDIS_REPLY_ARRAY) {
            if (reply->elements == 2) {
              redisReply* rcounter = reply->element[0];
              count_it = std::stoi(rcounter->str);
              if (count_it == 0) {
                end_it = true;
              }
              redisReply* keys = reply->element[1];
              unsigned int num_keys = keys->elements;
              for (unsigned int i = 0; i < num_keys; ++i) {
                redisReply* key = keys->element[i];
                if (key->type == REDIS_REPLY_STRING) {
                  redisAppendCommand(redis_context, "GET %b", key->str, key->len);
                }
              }
              for (unsigned int i = 0; i < num_keys; ++i) {
                redisReply* get_reply = NULL;
                redisReply* key = keys->element[i];
                redisGetReply(redis_context, (void**) &get_reply);
                if (get_reply != NULL) {
                  if (get_reply->type == REDIS_REPLY_ERROR) {
                    errlog("loadPersistEntries: Error getting key %b in persistent redis BlackWhiteListDB (%s)",
                           key->str, key->len, get_reply->str);
                    retval = false;
                  }
                  else if (get_reply->type == REDIS_REPLY_STRING) {
                    std::string keystr(key->str, key->len);
                    std::string valstr(get_reply->str, get_reply->len);

                    // keystr looks like <redis_prefix>:ip_bl:key
                    pair<string, string> headersplit = splitField(keystr, ':');
                    pair<string, string> keysplit = splitField(headersplit.second, ':');
                    // valstr looks like seconds:reason
                    pair<string, string> valsplit = splitField(valstr, ':');
                    std::string blwl_name = keysplit.first;
                    std::string blwl_key = keysplit.second;
                    time_t blwl_seconds = std::stoi(valsplit.first) - time(NULL);
                    std::string blwl_reason = valsplit.second;

                    BLWLType blwl_type = BLWLNameToType(blwl_name);
                    switch (blwl_type) {
                      case IP_BLWL:
                        _addEntry(blwl_key, blwl_seconds, ip_list, blwl_reason);
                        iplist_netmask.addMask(blwl_key);
                        ++num_entries;
                        break;
                      case LOGIN_BLWL:
                        _addEntry(blwl_key, blwl_seconds, login_list, blwl_reason);
                        ++num_entries;
                        break;
                      case IP_LOGIN_BLWL:
                        _addEntry(blwl_key, blwl_seconds, ip_login_list, blwl_reason);
                        ++num_entries;
                        break;
                      case JA3_BLWL:
                        _addEntry(blwl_key, blwl_seconds, ja3_list, blwl_reason);
                        ++num_entries;
                        break;
                      case IP_JA3_BLWL:
                        _addEntry(blwl_key, blwl_seconds, ip_ja3_list, blwl_reason);
                        ++num_entries;
                        break;
                      default:
                        errlog("loadPersistEntries: Error in blackwhitelist name retrieved from key: %s", blwl_name);
                        retval = false;
                    }
                  }
                  freeReplyObject(get_reply);
                }
                else {
                  retval = false;
                  errlog("loadPersistEntries: Redis GET failed: %s", redis_context->errstr);
                }
                if (retval == false)
                  break;
              }
            }
          }
          freeReplyObject(reply);
        }
        else {
          retval = false;
          errlog("loadPersistEntries: Redis SCAN failed: %s", redis_context->errstr);
        }
        if (retval == false)
          break;
      }
    }
    else {
      retval = false;
      std::string myerr = "Error - could not connect to configured persistent redis DB (" + redis_server + ":" +
                          std::to_string(redis_port) + ")";
      errlog(myerr.c_str());
      throw WforceException(myerr);
    }
    if (retval == true)
      noticelog("Loaded %d blackwhitelist entries from persistent redis DB", num_entries);
  }
  return retval;
}

BLWLType BlackWhiteListDB::BLWLNameToType(const std::string& blwl_name) const
{
  int i=0;
  for (auto &key_name : key_names) {
    if (blwl_name.compare(key_name) == 0)
      return (BLWLType) i;
    i++;
  }
  return NONE_BLWL;
}

std::string BlackWhiteListDB::BLWLTypeToName(BLWLType blwl_type) const
{
  if (blwl_type < key_names.size())
    return key_names[blwl_type];
  else
    return std::string();
}
