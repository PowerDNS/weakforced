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

#include "blacklist.hh"
#include "replication.hh"
#include "replication_bl.hh"
#include "replication.pb.h"
#include "wforce.hh"
#include <boost/version.hpp>
#include <boost/date_time/posix_time/ptime.hpp>
#include <boost/date_time/gregorian/gregorian.hpp>
#include <boost/date_time/posix_time/posix_time_duration.hpp>
#include <hiredis/hiredis.h>
#include <ostream>

BlackListDB g_bl_db;

std::string BlackListDB::ipLoginStr(const ComboAddress& ca, const std::string& login)
{
  return ca.toString() + ":" + login;
}

void BlackListDB::addEntry(const ComboAddress& ca, time_t seconds, const std::string& reason)
{
  std::string key = ca.toString();
  
  addEntryInternal(key, seconds, IP_BL, reason, true);
  addEntryLog(IP_BL, key, seconds, reason);
}

void BlackListDB::addEntry(const std::string& login, time_t seconds, const std::string& reason)
{
  addEntryInternal(login, seconds, LOGIN_BL, reason, true);
  addEntryLog(LOGIN_BL, login, seconds, reason);
}

void BlackListDB::addEntry(const ComboAddress& ca, const std::string& login, time_t seconds, const std::string& reason)
{
  std::string key = ipLoginStr(ca, login);
  addEntryInternal(key, seconds, IP_LOGIN_BL, reason, true);
  addEntryLog(IP_LOGIN_BL, key, seconds, reason);
}

void BlackListDB::addEntryInternal(const std::string& key, time_t seconds, BLType bl_type, const std::string& reason, bool replicate)
{
  std::lock_guard<std::mutex> lock(mutx);

  switch (bl_type) {
  case IP_BL:
    _addEntry(key, seconds, ip_blacklist, reason);
    addEntryLog(IP_BL, key, seconds, reason);
    if (persist && ((replicate == true) || ((replicate == false) && persist_replicated)))
      addPersistEntry(key, seconds, IP_BL, reason);
    if (replicate == true) {
      std::shared_ptr<BLReplicationOperation> bl_rop;
      bl_rop = std::make_shared<BLReplicationOperation>(BLOperation_BLOpType_BLAdd, IP_BL, key, seconds, reason);
      ReplicationOperation rep_op(bl_rop, WforceReplicationMsg_RepType_BlacklistType);
      replicateOperation(rep_op);
    }
    break;
  case LOGIN_BL:
    _addEntry(key, seconds, login_blacklist, reason);
    addEntryLog(LOGIN_BL, key, seconds, reason);
    if (persist && ((replicate == true) || ((replicate == false) && persist_replicated)))
      addPersistEntry(key, seconds, LOGIN_BL, reason);
    if (replicate == true) {
      std::shared_ptr<BLReplicationOperation> bl_rop;
      bl_rop = std::make_shared<BLReplicationOperation>(BLOperation_BLOpType_BLAdd, LOGIN_BL, key, seconds, reason);
      ReplicationOperation rep_op(bl_rop, WforceReplicationMsg_RepType_BlacklistType);
      replicateOperation(rep_op);
    }
    break;
  case IP_LOGIN_BL:
    _addEntry(key, seconds, ip_login_blacklist, reason);
    addEntryLog(IP_LOGIN_BL, key, seconds, reason);
    if (persist && ((replicate == true) || ((replicate == false) && persist_replicated)))
      addPersistEntry(key, seconds, IP_LOGIN_BL, reason);
    if (replicate == true) {
      std::shared_ptr<BLReplicationOperation> bl_rop;
      bl_rop = std::make_shared<BLReplicationOperation>(BLOperation_BLOpType_BLAdd, IP_LOGIN_BL, key, seconds, reason);
      ReplicationOperation rep_op(bl_rop, WforceReplicationMsg_RepType_BlacklistType);
      replicateOperation(rep_op);
    }
    break;
  default:
    break;
  }
  // only generate webhook for this event for the first add, not for replicas
  if (replicate == true) {
    Json jobj = Json::object{{"key", key}, {"bl_type", BLTypeToName(bl_type)}, {"reason", reason}, {"expire_secs", (int)seconds}};
    std::string hook_data = jobj.dump();
    for (const auto& h : g_webhook_db.getWebHooksForEvent("addbl")) {
      g_webhook_runner.runHook("addbl", h, hook_data);
    }
  }
}

void BlackListDB::_addEntry(const std::string& key, time_t seconds, blacklist_t& blacklist, const std::string& reason)
{
  BlackListEntry bl;

  bl.key = key;
  bl.expiration = boost::posix_time::from_time_t(time(NULL)) + boost::posix_time::seconds(seconds);
  bl.reason = reason;

  auto& keyindex = blacklist.get<KeyTag>();
  keyindex.erase(key);

  blacklist.insert(bl);
}

bool BlackListDB::checkEntry(const ComboAddress& ca)
{
  return _checkEntry(ca.toString(), ip_blacklist);
}

bool BlackListDB::checkEntry(const std::string& login)
{
  return _checkEntry(login, login_blacklist);
}

bool BlackListDB::checkEntry(const ComboAddress& ca, const std::string& login)
{
  return _checkEntry(ipLoginStr(ca, login), ip_login_blacklist);
}

bool BlackListDB::_checkEntry(const std::string& key, const blacklist_t& blacklist)
{
  std::lock_guard<std::mutex> lock(mutx);
  
  auto& keyindex = blacklist.get<KeyTag>();
  auto kit = keyindex.find(key);

  if (kit != keyindex.end())
    return true;
  return false;
}

bool BlackListDB::getEntry(const ComboAddress& ca, BlackListEntry& ret)
{
  return _getEntry(ca.toString(), ip_blacklist, ret);
} 

bool BlackListDB::getEntry(const std::string& login, BlackListEntry& ret)
{
  return _getEntry(login, login_blacklist, ret);
} 

bool BlackListDB::getEntry(const ComboAddress& ca, const std::string& login, BlackListEntry& ret)
{
  return _getEntry(ipLoginStr(ca, login), ip_login_blacklist, ret);
} 

bool BlackListDB::_getEntry(const std::string& key, blacklist_t& blacklist, BlackListEntry& ret_ble)
{
  std::lock_guard<std::mutex> lock(mutx);

  auto& keyindex = blacklist.get<KeyTag>();
  auto kit = keyindex.find(key);

  if (kit != keyindex.end()) {
    ret_ble = *kit; // copy
    return true;
  }
  return false;
}

void BlackListDB::deleteEntry(const ComboAddress& ca)
{
  std::string key = ca.toString();

  deleteEntryInternal(key, IP_BL, true);
}

void BlackListDB::deleteEntry(const std::string& login)
{
  deleteEntryInternal(login, LOGIN_BL, true);
}

void BlackListDB::deleteEntry(const ComboAddress& ca, const std::string& login)
{
  std::string key = ipLoginStr(ca, login);

  deleteEntryInternal(key, IP_LOGIN_BL, true);
}

void BlackListDB::deleteEntryInternal(const std::string& key, BLType bl_type, bool replicate)
{
  std::lock_guard<std::mutex> lock(mutx);

  switch (bl_type) {
  case IP_BL:
    deleteEntryLog(IP_BL, key);
    _deleteEntry(key, ip_blacklist);
    if (persist && ((replicate == true) || ((replicate == false) && persist_replicated))) {
      deletePersistEntry(key, bl_type, ip_blacklist);
    }
    if (replicate == true) {
      std::shared_ptr<BLReplicationOperation> bl_rop;
      bl_rop = std::make_shared<BLReplicationOperation>(BLOperation_BLOpType_BLDelete, IP_BL, key, 0, "");
      ReplicationOperation rep_op(bl_rop, WforceReplicationMsg_RepType_BlacklistType);
      replicateOperation(rep_op);
    }
    break;
  case LOGIN_BL:
    deleteEntryLog(LOGIN_BL, key);
    _deleteEntry(key, login_blacklist);
    if (persist && ((replicate == true) || ((replicate == false) && persist_replicated))) {
      deletePersistEntry(key, bl_type, login_blacklist);
    }
    if (replicate == true) {
      std::shared_ptr<BLReplicationOperation> bl_rop;
      bl_rop = std::make_shared<BLReplicationOperation>(BLOperation_BLOpType_BLDelete, LOGIN_BL, key, 0, "");
      ReplicationOperation rep_op(bl_rop, WforceReplicationMsg_RepType_BlacklistType);
      replicateOperation(rep_op);
    }
    break;
  case IP_LOGIN_BL:
    deleteEntryLog(IP_LOGIN_BL, key);
    _deleteEntry(key, ip_login_blacklist);
    if (persist && ((replicate == true) || ((replicate == false) && persist_replicated))) {
      deletePersistEntry(key, bl_type, ip_login_blacklist);
    }
    if (replicate == true) {
      std::shared_ptr<BLReplicationOperation> bl_rop;
      bl_rop = std::make_shared<BLReplicationOperation>(BLOperation_BLOpType_BLDelete, IP_LOGIN_BL, key, 0, "");
      ReplicationOperation rep_op(bl_rop, WforceReplicationMsg_RepType_BlacklistType);
      replicateOperation(rep_op);
    }
    break;
  default:
    break;
  }
  // only generate webhook for this event for the first delete, not for replicas
  if (replicate == true) {
    Json jobj = Json::object{{"key", key}, {"bl_type", BLTypeToName(bl_type)}};
    std::string hook_data = jobj.dump();
    for (const auto& h : g_webhook_db.getWebHooksForEvent("delbl")) {
      g_webhook_runner.runHook("delbl", h, hook_data);
    }
  }

}

bool BlackListDB::_deleteEntry(const std::string& key, blacklist_t& blacklist)
{
  auto& keyindex = blacklist.get<KeyTag>();
  auto kit = keyindex.find(key);

  if (kit != keyindex.end()) {
    keyindex.erase(key);
    return true;
  }
  return false;  
}

time_t BlackListDB::getExpiration(const ComboAddress& ca)
{
  return _getExpiration(ca.toString(), ip_blacklist);
}

time_t BlackListDB::getExpiration(const std::string& login)
{
  return _getExpiration(login, login_blacklist);
}

time_t BlackListDB::getExpiration(const ComboAddress& ca, const std::string& login)
{
  return _getExpiration(ipLoginStr(ca, login), ip_login_blacklist);
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

time_t BlackListDB::_getExpiration(const std::string& key, blacklist_t& blacklist)
{
  std::lock_guard<std::mutex> lock(mutx);

  auto& keyindex = blacklist.get<KeyTag>();
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

void BlackListDB::purgeEntries()
{
  while (true) {
    sleep(1);
    _purgeEntries(IP_BL, ip_blacklist, IP_BL);
    _purgeEntries(LOGIN_BL, login_blacklist, LOGIN_BL);
    _purgeEntries(IP_LOGIN_BL, ip_login_blacklist, IP_LOGIN_BL);
  }
}

void BlackListDB::_purgeEntries(BLType blt, blacklist_t& blacklist, BLType bl_type)
{
  std::lock_guard<std::mutex> lock(mutx);
  boost::system_time now = boost::get_system_time();
  
  auto& timeindex = blacklist.get<TimeTag>();
  
  for (auto tit = timeindex.begin(); tit != timeindex.end();) {
    if (tit->expiration <= now) {
      Json jobj = Json::object{{"key", tit->key}, {"bl_type", BLTypeToName(bl_type)}};
      std::string hook_data = jobj.dump();
      for (const auto& h : g_webhook_db.getWebHooksForEvent("expirebl")) {
	g_webhook_runner.runHook("expirebl", h, hook_data);
      }
      expireEntryLog(blt, tit->key);
      tit = timeindex.erase(tit);
    }
    else
      break; // stop when we run out of entries to expire
  }
}

std::vector<BlackListEntry> BlackListDB::getIPEntries()
{
  std::lock_guard<std::mutex> lock(mutx);
  std::vector<BlackListEntry> ret;

  auto& seqindex = ip_blacklist.get<SeqTag>();  
  for(const auto& a : seqindex)
    ret.push_back(a);
  return ret;
}

std::vector<BlackListEntry> BlackListDB::getLoginEntries()
{
  std::lock_guard<std::mutex> lock(mutx);
  std::vector<BlackListEntry> ret;

  auto& seqindex = login_blacklist.get<SeqTag>();  
  for(const auto& a : seqindex)
    ret.push_back(a); 
  return ret;
}

std::vector<BlackListEntry> BlackListDB::getIPLoginEntries()
{
  std::lock_guard<std::mutex> lock(mutx);
  std::vector<BlackListEntry> ret;

  auto& seqindex = ip_login_blacklist.get<SeqTag>();  
  for(const auto& a : seqindex)
    ret.push_back(a);
  return ret;
}

void BlackListDB::addEntryLog(BLType blt, const std::string& key, time_t seconds, const std::string& reason)
{
  std::ostringstream os;
  std::string bl_name = string(bl_names[blt]);
  std::string key_name = string(key_names[blt]);

  os << "addBLEntry " + bl_name + ": " + key_name + "=" + key + " expire_secs=" + std::to_string(seconds) + " reason=\"" + reason + "\"";
  noticelog(os.str().c_str());
}

void BlackListDB::deleteEntryLog(BLType blt, const std::string& key)
{
  std::ostringstream os;
  std::string bl_name = string(bl_names[blt]);
  std::string key_name = string(key_names[blt]);

  os << "deleteBLEntry " + bl_name + ": " + key_name + "=" + key;
  noticelog(os.str().c_str());
}

void BlackListDB::expireEntryLog(BLType blt, const std::string& key)
{
  std::ostringstream os;
  std::string bl_name = string(bl_names[blt]);
  std::string key_name = string(key_names[blt]);

  os << "expireBLEntry " + bl_name + ": " + key_name + "=" + key;
  noticelog(os.str().c_str());
}

bool BlackListDB::checkSetupContext()
{
  if (redis_context == NULL || redis_context->err) {
    if (redis_context)
      redisFree(redis_context);
    // something went wrong previously, try again
    redis_context = redisConnect(redis_server.c_str(), redis_port);
    if (redis_context == NULL || redis_context->err) {
      if (redis_context) {
	redisFree(redis_context);
	redis_context = NULL;
      }
      errlog("checkSetupContext: could not connect to redis BlackListDB (%s:%d)", redis_server, redis_port);
      return false;
    }
  }
  return true;
}

void BlackListDB::makePersistent(const std::string& host, unsigned int port=6379)
{
  persist = true;
  redis_server = host;
  redis_port = port;
}

bool BlackListDB::addPersistEntry(const std::string& key, time_t seconds, BLType bl_type, const std::string& reason)
{
  bool retval = false;
  if (checkSetupContext()) {
    std::stringstream redis_key_s;
    std::stringstream redis_value_s;
    std::string bl_key = string(key_names[bl_type]);
    time_t expiration_time = time(NULL) + seconds;
    
    redis_key_s << "wfbl:" << bl_key << ":" << key;
    redis_value_s << expiration_time << ":" << reason;
    redisReply* reply = (redisReply*)redisCommand(redis_context, "SET %s %b EX %d", redis_key_s.str().c_str(), redis_value_s.str().c_str(), redis_value_s.str().length(), seconds);
    if (reply != NULL) {
      if (reply->type == REDIS_REPLY_ERROR) {
	errlog("addPersistEntry: Error adding %s to persistent redis BlackListDB (%s)", key, reply->str);
      }
      else
	retval = true;
      freeReplyObject(reply);
    }
  }
  return retval;
}

bool BlackListDB::deletePersistEntry(const std::string& key, BLType bl_type, blacklist_t& blacklist)
{
  bool retval = false;
  if (checkSetupContext()) {
    BlackListEntry ble;
    std::stringstream redis_key_s;
    std::string bl_key = string(key_names[bl_type]);
    
    redis_key_s << "wfbl:" << bl_key << ":" << key;
    redisReply* reply = (redisReply*)redisCommand(redis_context, "DEL %s", redis_key_s.str().c_str());
    if (reply != NULL) {
      if (reply->type == REDIS_REPLY_ERROR) {
	errlog("deletePersistEntry: Error deleting %s from persistent redis BlackListDB (%s)", key, reply->str);
      }
      else
	retval = true;
      freeReplyObject(reply);
    }
  }
  return retval;
}

bool BlackListDB::loadPersistEntries()
{
  bool retval = true;
  std::lock_guard<std::mutex> lock(mutx);
  
  if (persist != false) {
    unsigned int num_entries = 0;
    if (checkSetupContext()) {
      bool end_it = false;
      unsigned int count_it = 0;
      
      while (end_it != true) {
	redisReply* reply = (redisReply*)redisCommand(redis_context, "SCAN %d MATCH wfbl* COUNT 1000", count_it);
	if (reply != NULL) {
	  if (reply->type == REDIS_REPLY_ERROR) {
	    errlog("loadPersistEntries: Error scanning keys in persistent redis BlackListDB (%s)", reply->str);
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
		    errlog("loadPersistEntries: Error getting key %b in persistent redis BlackListDB (%s)", key->str, key->len, get_reply->str);
		    retval = false;
		  }
		  else if (get_reply->type == REDIS_REPLY_STRING) {
		    std::string keystr(key->str, key->len);
		    std::string valstr(get_reply->str, get_reply->len);

		    // keystr looks like wfbl:ip_bl:key
		    pair<string, string> headersplit=splitField(keystr, ':');
		    pair<string, string> keysplit=splitField(headersplit.second, ':');
		    // valstr looks like seconds:reason
		    pair<string, string> valsplit=splitField(valstr, ':');
		    std::string bl_name = keysplit.first;
		    std::string bl_key = keysplit.second;
		    time_t bl_seconds = std::stoi(valsplit.first) - time(NULL);
		    std::string bl_reason = valsplit.second;

		    BLType bl_type = BLNameToType(bl_name);
		    switch (bl_type) {
		    case IP_BL:
		      _addEntry(bl_key, bl_seconds, ip_blacklist, bl_reason);
		      ++num_entries;
		      break;
		    case LOGIN_BL:
		      _addEntry(bl_key, bl_seconds, login_blacklist, bl_reason);
		      ++num_entries;
		      break;
		    case IP_LOGIN_BL:
		      _addEntry(bl_key, bl_seconds, ip_login_blacklist, bl_reason);
		      ++num_entries;
		      break;
		    default:
		      errlog("loadPersistEntries: Error in blacklist name retrieved from key: %s", bl_name);
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
      errlog("Could not connect to configured persistent redis DB (%s:%d)", redis_server, redis_port);
    }
    if (retval == true)
      warnlog("Loaded %d blacklist entries from persistent redis DB", num_entries);
  }
  return retval;
}

BLType BlackListDB::BLNameToType(const std::string& bl_name)
{
  for (unsigned int i=0; key_names[i]!=NULL; i++) {
    if (bl_name.compare(key_names[i]) == 0)
      return (BLType)i;
  }
  return NONE_BL;
}

std::string BlackListDB::BLTypeToName(BLType bl_type)
{
  return std::string(bl_names[bl_type]);
}
