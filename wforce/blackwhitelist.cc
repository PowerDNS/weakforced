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

#include "blackwhitelist.hh"
#include "replication.hh"
#include "replication_bl.hh"
#include "replication_wl.hh"
#include "replication.pb.h"
#include "wforce.hh"
#include <boost/version.hpp>
#include <boost/date_time/posix_time/ptime.hpp>
#include <boost/date_time/gregorian/gregorian.hpp>
#include <boost/date_time/posix_time/posix_time_duration.hpp>
#include <hiredis/hiredis.h>
#include <ostream>

BlackWhiteListDB g_bl_db(BLWLDBType::BLACKLIST);
BlackWhiteListDB g_wl_db(BLWLDBType::WHITELIST);

std::string BlackWhiteListDB::ipLoginStr(const ComboAddress& ca, const std::string& login) const
{
  return ca.toString() + ":" + login;
}

void BlackWhiteListDB::addEntry(const Netmask& nm, time_t seconds, const std::string& reason)
{
  std::string key = nm.toStringNetwork();

  addEntryInternal(key, seconds, IP_BLWL, reason, true);
  addEntryLog(IP_BLWL, key, seconds, reason);
}

void BlackWhiteListDB::addEntry(const ComboAddress& ca, time_t seconds, const std::string& reason)
{
  std::string key = Netmask(ca).toStringNetwork();
  
  addEntryInternal(key, seconds, IP_BLWL, reason, true);
  addEntryLog(IP_BLWL, key, seconds, reason);
}

void BlackWhiteListDB::addEntry(const std::string& login, time_t seconds, const std::string& reason)
{
  addEntryInternal(login, seconds, LOGIN_BLWL, reason, true);
  addEntryLog(LOGIN_BLWL, login, seconds, reason);
}

void BlackWhiteListDB::addEntry(const ComboAddress& ca, const std::string& login, time_t seconds, const std::string& reason)
{
  std::string key = ipLoginStr(ca, login);
  addEntryInternal(key, seconds, IP_LOGIN_BLWL, reason, true);
  addEntryLog(IP_LOGIN_BLWL, key, seconds, reason);
}

void BlackWhiteListDB::addEntryInternal(const std::string& key, time_t seconds, BLWLType blwl_type, const std::string& reason, bool replicate)
{
  WriteLock wl(&rwlock);

  switch (blwl_type) {
  case IP_BLWL:
    _addEntry(key, seconds, ip_list, reason);
    iplist_netmask.addMask(key);
    addEntryLog(IP_BLWL, key, seconds, reason);
    if (persist && ((replicate == true) || ((replicate == false) && persist_replicated)))
      addPersistEntry(key, seconds, IP_BLWL, reason);
    if (replicate == true) {
      if (db_type == BLWLDBType::BLACKLIST) {
        std::shared_ptr<BLReplicationOperation> bl_rop;
        bl_rop = std::make_shared<BLReplicationOperation>(BLOperation_BLOpType_BLAdd, IP_BLWL, key, seconds, reason);
        ReplicationOperation rep_op(bl_rop, WforceReplicationMsg_RepType_BlacklistType);
        replicateOperation(rep_op);
      }
      else {
        std::shared_ptr<WLReplicationOperation> wl_rop;
        wl_rop = std::make_shared<WLReplicationOperation>(BLOperation_BLOpType_BLAdd, IP_BLWL, key, seconds, reason);
        ReplicationOperation rep_op(wl_rop, WforceReplicationMsg_RepType_WhitelistType);
        replicateOperation(rep_op);
      }
    }
    break;
  case LOGIN_BLWL:
    _addEntry(key, seconds, login_list, reason);
    addEntryLog(LOGIN_BLWL, key, seconds, reason);
    if (persist && ((replicate == true) || ((replicate == false) && persist_replicated)))
      addPersistEntry(key, seconds, LOGIN_BLWL, reason);
    if (replicate == true) {
      if (db_type == BLWLDBType::BLACKLIST) {
        std::shared_ptr<BLReplicationOperation> bl_rop;
        bl_rop = std::make_shared<BLReplicationOperation>(BLOperation_BLOpType_BLAdd, LOGIN_BLWL, key, seconds, reason);
        ReplicationOperation rep_op(bl_rop, WforceReplicationMsg_RepType_BlacklistType);
        replicateOperation(rep_op);
      }
      else {
        std::shared_ptr<WLReplicationOperation> wl_rop;
        wl_rop = std::make_shared<WLReplicationOperation>(BLOperation_BLOpType_BLAdd, LOGIN_BLWL, key, seconds, reason);
        ReplicationOperation rep_op(wl_rop, WforceReplicationMsg_RepType_WhitelistType);
        replicateOperation(rep_op);
      }
    }
    break;
  case IP_LOGIN_BLWL:
    _addEntry(key, seconds, ip_login_list, reason);
    addEntryLog(IP_LOGIN_BLWL, key, seconds, reason);
    if (persist && ((replicate == true) || ((replicate == false) && persist_replicated)))
      addPersistEntry(key, seconds, IP_LOGIN_BLWL, reason);
    if (replicate == true) {
      if (db_type == BLWLDBType::BLACKLIST) {
        std::shared_ptr<BLReplicationOperation> bl_rop;
        bl_rop = std::make_shared<BLReplicationOperation>(BLOperation_BLOpType_BLAdd, IP_LOGIN_BLWL, key, seconds, reason);
        ReplicationOperation rep_op(bl_rop, WforceReplicationMsg_RepType_BlacklistType);
        replicateOperation(rep_op);
      }
      else {
        std::shared_ptr<WLReplicationOperation> wl_rop;
        wl_rop = std::make_shared<WLReplicationOperation>(BLOperation_BLOpType_BLAdd, IP_LOGIN_BLWL, key, seconds, reason);
        ReplicationOperation rep_op(wl_rop, WforceReplicationMsg_RepType_WhitelistType);
        replicateOperation(rep_op);
      }
    }
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
    Json jobj = Json::object{{"key", key}, {type_name, BLWLTypeToName(blwl_type)}, {"reason", reason}, {"expire_secs", (int)seconds}};
    std::string hook_data = jobj.dump();
    for (const auto& h : g_webhook_db.getWebHooksForEvent(event_name)) {
      if (auto hs = h.lock())
	g_webhook_runner.runHook(event_name, hs, hook_data);
    }
  }
}

void BlackWhiteListDB::_addEntry(const std::string& key, time_t seconds, blackwhitelist_t& blackwhitelist, const std::string& reason)
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
  return _checkEntry(ipLoginStr(ca, login), ip_login_list);
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
  return _getEntry(ipLoginStr(ca, login), ip_login_list, ret);
} 

bool BlackWhiteListDB::_getEntry(const std::string& key, const blackwhitelist_t& blackwhitelist, BlackWhiteListEntry& ret_ble) const
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
  std::string key = Netmask(ca).toStringNetwork();

  deleteEntryInternal(key, IP_BLWL, true);
}

void BlackWhiteListDB::deleteEntry(const std::string& login)
{
  deleteEntryInternal(login, LOGIN_BLWL, true);
}

void BlackWhiteListDB::deleteEntry(const ComboAddress& ca, const std::string& login)
{
  std::string key = ipLoginStr(ca, login);

  deleteEntryInternal(key, IP_LOGIN_BLWL, true);
}

void BlackWhiteListDB::deleteEntryInternal(const std::string& key, BLWLType blwl_type, bool replicate)
{
  WriteLock wl(&rwlock);

  switch (blwl_type) {
  case IP_BLWL:
    deleteEntryLog(IP_BLWL, key);
    _deleteEntry(key, ip_list);
    iplist_netmask.deleteMask(key);
    if (persist && ((replicate == true) || ((replicate == false) && persist_replicated))) {
      deletePersistEntry(key, blwl_type, ip_list);
    }
    if (replicate == true) {
      if (db_type == BLWLDBType::BLACKLIST) {
        std::shared_ptr<BLReplicationOperation> bl_rop;
        bl_rop = std::make_shared<BLReplicationOperation>(BLOperation_BLOpType_BLDelete, IP_BLWL, key, 0, "");
        ReplicationOperation rep_op(bl_rop, WforceReplicationMsg_RepType_BlacklistType);
        replicateOperation(rep_op);
      }
      else {
        std::shared_ptr<WLReplicationOperation> wl_rop;
        wl_rop = std::make_shared<WLReplicationOperation>(BLOperation_BLOpType_BLDelete, IP_BLWL, key, 0, "");
        ReplicationOperation rep_op(wl_rop, WforceReplicationMsg_RepType_WhitelistType);
        replicateOperation(rep_op);
      }
    }
    break;
  case LOGIN_BLWL:
    deleteEntryLog(LOGIN_BLWL, key);
    _deleteEntry(key, login_list);
    if (persist && ((replicate == true) || ((replicate == false) && persist_replicated))) {
      deletePersistEntry(key, blwl_type, login_list);
    }
    if (replicate == true) {
      if (db_type == BLWLDBType::BLACKLIST) {
        std::shared_ptr<BLReplicationOperation> bl_rop;
        bl_rop = std::make_shared<BLReplicationOperation>(BLOperation_BLOpType_BLDelete, LOGIN_BLWL, key, 0, "");
        ReplicationOperation rep_op(bl_rop, WforceReplicationMsg_RepType_BlacklistType);
        replicateOperation(rep_op);
      }
      else {
        std::shared_ptr<WLReplicationOperation> wl_rop;
        wl_rop = std::make_shared<WLReplicationOperation>(BLOperation_BLOpType_BLDelete, LOGIN_BLWL, key, 0, "");
        ReplicationOperation rep_op(wl_rop, WforceReplicationMsg_RepType_WhitelistType);
        replicateOperation(rep_op);
      }
    }
    break;
  case IP_LOGIN_BLWL:
    deleteEntryLog(IP_LOGIN_BLWL, key);
    _deleteEntry(key, ip_login_list);
    if (persist && ((replicate == true) || ((replicate == false) && persist_replicated))) {
      deletePersistEntry(key, blwl_type, ip_login_list);
    }
    if (replicate == true) {
      if (db_type == BLWLDBType::BLACKLIST) {
        std::shared_ptr<BLReplicationOperation> bl_rop;
        bl_rop = std::make_shared<BLReplicationOperation>(BLOperation_BLOpType_BLDelete, IP_LOGIN_BLWL, key, 0, "");
        ReplicationOperation rep_op(bl_rop, WforceReplicationMsg_RepType_BlacklistType);
        replicateOperation(rep_op);
      }
      else {
        std::shared_ptr<WLReplicationOperation> wl_rop;
        wl_rop = std::make_shared<WLReplicationOperation>(BLOperation_BLOpType_BLDelete, IP_LOGIN_BLWL, key, 0, "");
        ReplicationOperation rep_op(wl_rop, WforceReplicationMsg_RepType_WhitelistType);
        replicateOperation(rep_op);
      }
    }
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
    Json jobj = Json::object{{"key", key}, {type_name, BLWLTypeToName(blwl_type)}};
    std::string hook_data = jobj.dump();
    for (const auto& h : g_webhook_db.getWebHooksForEvent(event_name)) {
      if (auto hs = h.lock())
	g_webhook_runner.runHook(event_name, hs, hook_data);
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
  return _getExpiration(ipLoginStr(ca, login), ip_login_list);
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
  return (time_t)-1; // not found
}

void BlackWhiteListDB::purgeEntries()
{
  while (true) {
    sleep(1);
    _purgeEntries(IP_BLWL, ip_list, IP_BLWL);
    _purgeEntries(LOGIN_BLWL, login_list, LOGIN_BLWL);
    _purgeEntries(IP_LOGIN_BLWL, ip_login_list, IP_LOGIN_BLWL);
  }
}

void BlackWhiteListDB::_purgeEntries(BLWLType blt, blackwhitelist_t& blackwhitelist, BLWLType blwl_type)
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
      Json jobj = Json::object{{"key", tit->key}, {type_name, BLWLTypeToName(blwl_type)}};
      std::string hook_data = jobj.dump();
      for (const auto& h : g_webhook_db.getWebHooksForEvent(event_name)) {
	if (auto hs = h.lock())
	  g_webhook_runner.runHook(event_name, hs, hook_data);
      }
      expireEntryLog(blt, tit->key);
      if (blwl_type == IP_BLWL)
	iplist_netmask.deleteMask(tit->key);
      tit = timeindex.erase(tit);
    }
    else
      break; // stop when we run out of entries to expire
  }
}

std::vector<BlackWhiteListEntry> BlackWhiteListDB::getIPEntries() const
{
  ReadLock rl(&rwlock);
  std::vector<BlackWhiteListEntry> ret;

  auto& seqindex = ip_list.get<SeqTag>();  
  for(const auto& a : seqindex)
    ret.push_back(a);
  return ret;
}

std::vector<BlackWhiteListEntry> BlackWhiteListDB::getLoginEntries() const
{
  ReadLock rl(&rwlock);
  std::vector<BlackWhiteListEntry> ret;

  auto& seqindex = login_list.get<SeqTag>();  
  for(const auto& a : seqindex)
    ret.push_back(a); 
  return ret;
}

std::vector<BlackWhiteListEntry> BlackWhiteListDB::getIPLoginEntries() const
{
  ReadLock rl(&rwlock);
  std::vector<BlackWhiteListEntry> ret;

  auto& seqindex = ip_login_list.get<SeqTag>();  
  for(const auto& a : seqindex)
    ret.push_back(a);
  return ret;
}

void BlackWhiteListDB::addEntryLog(BLWLType blt, const std::string& key, time_t seconds, const std::string& reason) const
{
  std::ostringstream os;
  std::string blwl_name = list_names[blt];
  std::string key_name = string(key_names[blt]);
  std::string event_name;

  if (db_type == BLWLDBType::BLACKLIST)
    event_name = "addBLEntry";
  else
    event_name = "addWLEntry";
  
  os << event_name << " " << blwl_name << ": " << key_name << "=" << key << " expire_secs=" << std::to_string(seconds) << " reason=\"" << reason + "\"";
  noticelog(os.str().c_str());
}

void BlackWhiteListDB::deleteEntryLog(BLWLType blt, const std::string& key) const
{
  std::ostringstream os;
  std::string blwl_name = list_names[blt];
  std::string key_name = string(key_names[blt]);
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
  std::string key_name = string(key_names[blt]);
  std::string event_name;
  
  if (db_type == BLWLDBType::BLACKLIST)
    event_name = "deleteBLEntry";
  else
    event_name = "deleteWLEntry";

  os << event_name << " " << blwl_name << ": " << key_name << "=" << key;
  noticelog(os.str().c_str());
}

void BlackWhiteListDB::setConnectTimeout(int timeout)
{
  redis_timeout = timeout; // atomic
}

bool BlackWhiteListDB::checkSetupContext()
{
  if (redis_context == NULL || redis_context->err) {
    if (redis_context)
      redisFree(redis_context);
    // something went wrong previously, try again
    struct timeval tv;
    tv.tv_sec = redis_timeout;
    tv.tv_usec = 0;
    redis_context = redisConnectWithTimeout(redis_server.c_str(), redis_port, tv);
    if (redis_context == NULL || redis_context->err) {
      if (redis_context) {
	redisFree(redis_context);
	redis_context = NULL;
      }
      errlog("checkSetupContext: could not connect to redis BlackWhiteListDB (%s:%d)", redis_server, redis_port);
      return false;
    }
  }
  return true;
}

void BlackWhiteListDB::makePersistent(const std::string& host, unsigned int port=6379)
{
  persist = true;
  redis_server = host;
  redis_port = port;
}

bool BlackWhiteListDB::addPersistEntry(const std::string& key, time_t seconds, BLWLType blwl_type, const std::string& reason)
{
  bool retval = false;
  if (checkSetupContext()) {
    std::stringstream redis_key_s;
    std::stringstream redis_value_s;
    std::string blwl_key = string(key_names[blwl_type]);
    time_t expiration_time = time(NULL) + seconds;
    
    redis_key_s << redis_prefix << ":" << blwl_key << ":" << key;
    redis_value_s << expiration_time << ":" << reason;
    redisReply* reply = (redisReply*)redisCommand(redis_context, "SET %s %b EX %d", redis_key_s.str().c_str(), redis_value_s.str().c_str(), redis_value_s.str().length(), seconds);
    if (reply != NULL) {
      if (reply->type == REDIS_REPLY_ERROR) {
	errlog("addPersistEntry: Error adding %s to persistent redis BlackWhiteListDB (%s)", key, reply->str);
      }
      else
	retval = true;
      freeReplyObject(reply);
    }
  }
  return retval;
}

bool BlackWhiteListDB::deletePersistEntry(const std::string& key, BLWLType blwl_type, blackwhitelist_t& blackwhitelist)
{
  bool retval = false;
  if (checkSetupContext()) {
    BlackWhiteListEntry ble;
    std::stringstream redis_key_s;
    std::string blwl_key = string(key_names[blwl_type]);
    
    redis_key_s << redis_prefix << ":" << blwl_key << ":" << key;
    redisReply* reply = (redisReply*)redisCommand(redis_context, "DEL %s", redis_key_s.str().c_str());
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
	redisReply* reply = (redisReply*)redisCommand(redis_context, oss.str().c_str(), count_it);
	if (reply != NULL) {
	  if (reply->type == REDIS_REPLY_ERROR) {
	    errlog("loadPersistEntries: Error scanning keys in persistent redis BlackWhiteListDB (%s)", reply->str);
	    retval = false;
	  }
	  else if (reply->type == REDIS_REPLY_ARRAY) {
	    if (reply->elements == 2) {
	      redisReply* rcounter = reply->element[0];
	      count_it = rcounter->integer;
	      if (count_it == 0) {
		end_it = true;
	      }
	      redisReply* keys = reply->element[1];
	      unsigned int num_keys = keys->elements;
	      for (unsigned int i=0; i<num_keys; ++i) {
		redisReply* key = keys->element[i];
		if (key->type == REDIS_REPLY_STRING) {
		  redisAppendCommand(redis_context, "GET %b", key->str, key->len);
		}
	      }
	      for (unsigned int i=0; i<num_keys; ++i) {
		redisReply* get_reply = NULL;
		redisReply* key = keys->element[i];
		redisGetReply(redis_context, (void**)&get_reply);
		if (get_reply != NULL) {
		  if (get_reply->type == REDIS_REPLY_ERROR) {
		    errlog("loadPersistEntries: Error getting key %b in persistent redis BlackWhiteListDB (%s)", key->str, key->len, get_reply->str);
		    retval = false;
		  }
		  else if (get_reply->type == REDIS_REPLY_STRING) {
		    std::string keystr(key->str, key->len);
		    std::string valstr(get_reply->str, get_reply->len);

		    // keystr looks like <redis_prefix>:ip_bl:key
		    pair<string, string> headersplit=splitField(keystr, ':');
		    pair<string, string> keysplit=splitField(headersplit.second, ':');
		    // valstr looks like seconds:reason
		    pair<string, string> valsplit=splitField(valstr, ':');
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
		    default:
		      errlog("loadPersistEntries: Error in blackwhitelist name retrieved from key: %s", blwl_name);
		      retval = false;
		    }
		  }
		  freeReplyObject(get_reply);
		}
		if (retval == false)
		  break;
	      }
	    }
	  }
	  freeReplyObject(reply);
	}
	if (retval == false)
	  break;
      }
    }
    else {
      retval = false;
      std::string myerr = "Error - could not connect to configured persistent redis DB (" + redis_server + ":" + std::to_string(redis_port);
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
  for (unsigned int i=0; key_names[i]!=NULL; i++) {
    if (blwl_name.compare(key_names[i]) == 0)
      return (BLWLType)i;
  }
  return NONE_BLWL;
}

std::string BlackWhiteListDB::BLWLTypeToName(BLWLType blwl_type) const
{
  if (blwl_type < list_names.size())
    return list_names[blwl_type];
  else
    return std::string();
}
