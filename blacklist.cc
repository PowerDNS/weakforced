#include "blacklist.hh"
#include <boost/version.hpp>
#include <boost/date_time/posix_time/ptime.hpp>
#include <boost/date_time/gregorian/gregorian.hpp>
#include <boost/date_time/posix_time/posix_time_duration.hpp>

BlackListDB bl_db;

std::string BlackListDB::ipLoginStr(const ComboAddress& ca, const std::string& login)
{
  return ca.toString() + ":" + login;
}

void BlackListDB::addEntry(const ComboAddress& ca, time_t seconds, const std::string& reason)
{
  _addEntry(ca.toString(), seconds, ip_blacklist, reason);
  addEntryLog(IP_BL, ca.toString(), seconds, reason);
}

void BlackListDB::addEntry(const std::string& login, time_t seconds, const std::string& reason)
{
  _addEntry(login, seconds, login_blacklist, reason);
  addEntryLog(LOGIN_BL, login, seconds, reason);
}

void BlackListDB::addEntry(const ComboAddress& ca, const std::string& login, time_t seconds, const std::string& reason)
{
  std::string key = ipLoginStr(ca, login);
  _addEntry(key, seconds, ip_login_blacklist, reason);
  addEntryLog(IP_LOGIN_BL, key, seconds, reason);
}


void BlackListDB::_addEntry(const std::string& key, time_t seconds, blacklist_t& blacklist, const std::string& reason)
{
  BlackListEntry bl;

  std::lock_guard<std::mutex> lock(mutx);

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

bool BlackListDB::_checkEntry(const std::string& key, blacklist_t& blacklist)
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

bool BlackListDB::deleteEntry(const ComboAddress& ca)
{
  deleteEntryLog(IP_BL, ca.toString());
  return _deleteEntry(ca.toString(), ip_blacklist);
}

bool BlackListDB::deleteEntry(const std::string& login)
{
  deleteEntryLog(LOGIN_BL, login);
  return _deleteEntry(login, login_blacklist);
}

bool BlackListDB::deleteEntry(const ComboAddress& ca, const std::string& login)
{
  std::string key = ipLoginStr(ca, login);
  deleteEntryLog(IP_LOGIN_BL, key);
  return _deleteEntry(key, ip_login_blacklist);
}

bool BlackListDB::_deleteEntry(const std::string& key, blacklist_t& blacklist)
{
  std::lock_guard<std::mutex> lock(mutx);

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
    _purgeEntries(IP_BL, ip_blacklist);
    _purgeEntries(LOGIN_BL, login_blacklist);
    _purgeEntries(IP_LOGIN_BL, ip_login_blacklist);
  }
}

void BlackListDB::_purgeEntries(BLType blt, blacklist_t& blacklist)
{
  std::lock_guard<std::mutex> lock(mutx);
  boost::system_time now = boost::get_system_time();
  
  auto& timeindex = blacklist.get<TimeTag>();
  
  for (auto tit = timeindex.begin(); tit != timeindex.end();) {
    if (tit->expiration <= now) {
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
  warnlog(os.str().c_str());
}

void BlackListDB::deleteEntryLog(BLType blt, const std::string& key)
{
  std::ostringstream os;
  std::string bl_name = string(bl_names[blt]);
  std::string key_name = string(key_names[blt]);

  os << "deleteBLEntry " + bl_name + ": " + key_name + "=" + key;
  warnlog(os.str().c_str());
}

void BlackListDB::expireEntryLog(BLType blt, const std::string& key)
{
  std::ostringstream os;
  std::string bl_name = string(bl_names[blt]);
  std::string key_name = string(key_names[blt]);

  os << "expireBLEntry " + bl_name + ": " + key_name + "=" + key;
  warnlog(os.str().c_str());
}


