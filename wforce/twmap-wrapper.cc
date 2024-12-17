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

#include "twmap-wrapper.hh"
#include "replication.hh"
#include "replication_sdb.hh"
#include "replication.pb.h"

TWStringStatsDBWrapper::TWStringStatsDBWrapper(const std::string& name, int w_size, int n_windows, int n_shards) : window_size(w_size), num_windows(n_windows), num_shards(n_shards), db_name(name)
{
  sdbvp = std::make_shared<std::vector<std::shared_ptr<TWStatsDB<std::string>>>>();
  for (int i=0; i<num_shards; ++i) {
    auto s = std::make_shared<TWStatsDB<std::string>>(name, w_size, n_windows);
    s->set_map_size_soft(ctwstats_map_size_soft/n_shards);
    sdbvp->push_back(s);
  }
  replicated = std::make_shared<bool>(false);
}

TWStringStatsDBWrapper::TWStringStatsDBWrapper(const std::string& name, int window_size, int num_windows) : TWStringStatsDBWrapper(name, window_size, num_windows, 1)
{
}

TWStringStatsDBWrapper::TWStringStatsDBWrapper(const std::string& name, int window_size, int num_windows, const std::vector<pair<std::string, std::string>>& fmvec, int num_shards) : TWStringStatsDBWrapper(name, window_size, num_windows, num_shards)
{
 (void)setFields(fmvec);
}

TWStringStatsDBWrapper::TWStringStatsDBWrapper(const std::string& name, int window_size, int num_windows, const std::vector<pair<std::string, std::string>>& fmvec) : TWStringStatsDBWrapper(name, window_size, num_windows, fmvec, 1)
{
}

void TWStringStatsDBWrapper::enableReplication()
{
  *replicated = true;
}

void TWStringStatsDBWrapper::disableReplication()
{
  *replicated = false;
}

bool TWStringStatsDBWrapper::getReplicationStatus()
{
  return *replicated;
}

std::string TWStringStatsDBWrapper::getDBName()
{
  return db_name;
}

bool TWStringStatsDBWrapper::setFields(const std::vector<pair<std::string, std::string>>& fmvec)
{
  for (const auto& sdbp : *sdbvp) {
    FieldMap fm;
    for(const auto& f : fmvec) {
      fm.insert(std::make_pair(f.first, f.second));
    }
    sdbp->setFields(fm);
  }
  return true;
}

bool TWStringStatsDBWrapper::setFields(const FieldMap& fm) 
{
  for (const auto& sdbp : *sdbvp) {
    sdbp->setFields(fm);
  }
  return true;
}

const FieldMap& TWStringStatsDBWrapper::getFields()
{
  // Just return the fields from the first
  return(sdbvp->at(0)->getFields());
}

void TWStringStatsDBWrapper::setv4Prefix(uint8_t bits)
{
  uint8_t v4_prefix = bits > 32 ? 32 : bits;
  for (const auto& sdbp : *sdbvp) {
    sdbp->setv4Prefix(v4_prefix);
  }
}

void TWStringStatsDBWrapper::setv6Prefix(uint8_t bits)
{
  uint8_t v6_prefix = bits > 128 ? 128 : bits;
  for (const auto& sdbp : *sdbvp) {
    sdbp->setv6Prefix(v6_prefix);
  }
}

std::string TWStringStatsDBWrapper::getStringKey(const TWKeyType vkey)
{
  if (vkey.which() == 0) {
    return boost::get<std::string>(vkey);
  }
  else if (vkey.which() == 1) {
    return std::to_string(boost::get<int>(vkey));
  }
  else if (vkey.which() == 2) {
    const ComboAddress ca =  boost::get<ComboAddress>(vkey);
    if (ca.isIpv4()) {
      uint8_t v4_prefix = sdbvp->at(0)->getv4Prefix();
      return Netmask(ca, v4_prefix).toStringNetwork();
    }
    else if (ca.isIpv6()) {
      uint8_t v6_prefix = sdbvp->at(0)->getv6Prefix();
      return Netmask(ca, v6_prefix).toStringNetwork();
    }
  }
  return std::string{};
}

void TWStringStatsDBWrapper::add(const TWKeyType vkey, const std::string& field_name, const boost::variant<std::string, int, ComboAddress>& param1, boost::optional<int> param2)
{
  addInternal(vkey, field_name, param1, param2, true);
}

const uint32_t tw_hash_seed = 623;

unsigned int TWStringStatsDBWrapper::getShardIndex(const std::string& key)
{
  uint32_t hash;
  MurmurHash3_x86_32(key.c_str(), key.length(), tw_hash_seed, (void*) &hash);
  return hash % num_shards;
}

void TWStringStatsDBWrapper::addInternal(const TWKeyType vkey, const std::string& field_name, const boost::variant<std::string, int, ComboAddress>& param1, boost::optional<int> param2, bool replicate)
{	
  std::string key = getStringKey(vkey);
  std::shared_ptr<SDBReplicationOperation> sdb_rop;
  std::shared_ptr<TWStatsDB<std::string>> sdbp = sdbvp->at(getShardIndex(key));
  
  // we're using the three argument version
  if (param2) {
    if ((param1.which() != 0) && (param1.which() != 2))
      return;
    else if (param1.which() == 0) {
      std::string mystr = boost::get<std::string>(param1);
      sdbp->add(key, field_name, mystr, *param2);
      if ((replicate == true) && (*replicated == true))
	sdb_rop = std::make_shared<SDBReplicationOperation>(db_name, SDBOperation_SDBOpType_SDBOpAdd, key, field_name, mystr, *param2);
    }
    else if (param1.which() == 2) {
      ComboAddress ca = boost::get<ComboAddress>(param1);
      sdbp->add(key, field_name, ca.toString(), *param2);
      if ((replicate == true) && (*replicated == true))
	sdb_rop = std::make_shared<SDBReplicationOperation>(db_name, SDBOperation_SDBOpType_SDBOpAdd, key, field_name, ca.toString(), *param2);
    }
  }
  else {
    if (param1.which() == 0) {
      std::string mystr = boost::get<std::string>(param1);
      sdbp->add(key, field_name, mystr);
      if ((replicate == true) && (*replicated == true))
	sdb_rop = std::make_shared<SDBReplicationOperation>(db_name, SDBOperation_SDBOpType_SDBOpAdd, key, field_name, mystr);
    }
    else if (param1.which() == 1) {
      int myint = boost::get<int>(param1);
      sdbp->add(key, field_name, myint);
      if ((replicate == true) && (*replicated == true))
	sdb_rop = std::make_shared<SDBReplicationOperation>(db_name, SDBOperation_SDBOpType_SDBOpAdd, key, field_name, myint);
    }
    else if (param1.which() == 2) {
      ComboAddress ca = boost::get<ComboAddress>(param1);
      sdbp->add(key, field_name, ca.toString());
      if ((replicate == true) && (*replicated == true))
	sdb_rop = std::make_shared<SDBReplicationOperation>(db_name, SDBOperation_SDBOpType_SDBOpAdd,
									 key, field_name, ca.toString());
    }
  }
  if ((replicate == true) && (*replicated == true)) {
    ReplicationOperation rep_op(sdb_rop, WforceReplicationMsg_RepType_SDBType);
    // this actually does the replication
    replicateOperation(rep_op);
  }
}

void TWStringStatsDBWrapper::sub(const TWKeyType vkey, const std::string& field_name, const boost::variant<std::string, int>& val)
{	
  subInternal(vkey, field_name, val, true);
}

void TWStringStatsDBWrapper::subInternal(const TWKeyType vkey, const std::string& field_name, const boost::variant<std::string, int>& val, bool replicate)
{
  std::string key = getStringKey(vkey);
  std::shared_ptr<SDBReplicationOperation> sdb_rop;
  std::shared_ptr<TWStatsDB<std::string>> sdbp = sdbvp->at(getShardIndex(key));

  if (val.which() == 0) {
    std::string mystr = boost::get<std::string>(val);
    sdbp->sub(key, field_name, mystr);
    if ((replicate == true) && (*replicated == true))
      sdb_rop = std::make_shared<SDBReplicationOperation>(db_name, SDBOperation_SDBOpType_SDBOpSub, key, field_name, mystr);
  }
  else {
    int myint = boost::get<int>(val);
    sdbp->sub(key, field_name, myint);
    if ((replicate == true) && (*replicated == true))
      sdb_rop = std::make_shared<SDBReplicationOperation>(db_name, SDBOperation_SDBOpType_SDBOpSub, key, field_name, myint);
  }
  if ((replicate == true) && (*replicated == true)) {
    ReplicationOperation rep_op(sdb_rop, WforceReplicationMsg_RepType_SDBType);
    // this actually does the replication
    replicateOperation(rep_op);
  }
}
  
int TWStringStatsDBWrapper::get(const TWKeyType vkey, const std::string& field_name, const boost::optional<boost::variant<std::string, ComboAddress>> param1)
{
  std::string key = getStringKey(vkey);
  std::shared_ptr<TWStatsDB<std::string>> sdbp = sdbvp->at(getShardIndex(key));

  if (param1) {
    if (param1->which() == 0) {
      std::string s = boost::get<std::string>(*param1);  
      return sdbp->get(key, field_name, s);
    }
    else {
      ComboAddress ca = boost::get<ComboAddress>(*param1);
      return sdbp->get(key, field_name, ca.toString());
    }
  }
  else {
    return sdbp->get(key, field_name);
  }
}

int TWStringStatsDBWrapper::get_current(const TWKeyType vkey, const std::string& field_name, const boost::optional<boost::variant<std::string, ComboAddress>> param1)
{
  std::string key = getStringKey(vkey);
  std::shared_ptr<TWStatsDB<std::string>> sdbp = sdbvp->at(getShardIndex(key));

  if (param1) {
    if (param1->which() == 0) {
      std::string s = boost::get<std::string>(*param1);  
      return sdbp->get_current(key, field_name, s);
    }
    else {
      ComboAddress ca = boost::get<ComboAddress>(*param1);
      return sdbp->get_current(key, field_name, ca.toString());
    }
  }
  else {
    return sdbp->get_current(key, field_name);
  }
}

std::vector<int> TWStringStatsDBWrapper::get_windows(const TWKeyType vkey, const std::string& field_name, const boost::optional<boost::variant<std::string, ComboAddress>> param1)
{
  std::vector<int> retvec;
    
  std::string key = getStringKey(vkey);
  std::shared_ptr<TWStatsDB<std::string>> sdbp = sdbvp->at(getShardIndex(key));

  if (param1) {
    if (param1->which() == 0) {
      std::string s = boost::get<std::string>(*param1);  
      (void) sdbp->get_windows(key, field_name, s, retvec);
    }
    else {
      ComboAddress ca = boost::get<ComboAddress>(*param1);
      (void) sdbp->get_windows(key, field_name, ca.toString(), retvec);
    }      
  }
  else
    (void) sdbp->get_windows(key, field_name, retvec);

  return retvec; // copy
}

bool TWStringStatsDBWrapper::get_all_fields(const TWKeyType vkey, std::vector<std::pair<std::string, int>>& ret_vec)
{
  std::string key = getStringKey(vkey);
  std::shared_ptr<TWStatsDB<std::string>> sdbp = sdbvp->at(getShardIndex(key));
  return sdbp->get_all_fields(key, ret_vec);
}

void TWStringStatsDBWrapper::reset(const TWKeyType vkey)
{
  resetInternal(vkey, true);
}

void TWStringStatsDBWrapper::resetInternal(const TWKeyType vkey, bool replicate)
{
  std::string key = getStringKey(vkey);
  std::shared_ptr<SDBReplicationOperation> sdb_rop;
  std::shared_ptr<TWStatsDB<std::string>> sdbp = sdbvp->at(getShardIndex(key));

  sdbp->reset(key);

  if ((replicate == true) && (*replicated == true)) {
    sdb_rop = std::make_shared<SDBReplicationOperation>(db_name, SDBOperation_SDBOpType_SDBOpReset, key);
    ReplicationOperation rep_op(sdb_rop, WforceReplicationMsg_RepType_SDBType);
    // this actually does the replication
    replicateOperation(rep_op);
  }
}

void TWStringStatsDBWrapper::resetField(const TWKeyType vkey, const std::string& field_name)
{
  resetFieldInternal(vkey, field_name, true);
}

void TWStringStatsDBWrapper::resetFieldInternal(const TWKeyType vkey, const std::string& field_name, bool replicate)
{
  std::string key = getStringKey(vkey);
  std::shared_ptr<SDBReplicationOperation> sdb_rop;
  std::shared_ptr<TWStatsDB<std::string>> sdbp = sdbvp->at(getShardIndex(key));

  sdbp->reset_field(key, field_name);

  if ((replicate == true) && (*replicated == true)) {
    sdb_rop = std::make_shared<SDBReplicationOperation>(db_name, SDBOperation_SDBOpType_SDBOpResetField, key, field_name);
    ReplicationOperation rep_op(sdb_rop, WforceReplicationMsg_RepType_SDBType);
    // this actually does the replication
    replicateOperation(rep_op);
  }
}


unsigned int TWStringStatsDBWrapper::get_size()
{	
  unsigned int size=0;
  for (const auto& sdbp : *sdbvp) {
    size += sdbp->get_size();
  }
  return size;
}

void TWStringStatsDBWrapper::set_size_soft(unsigned int size) 
{
  for (const auto& sdbp : *sdbvp) {
    sdbp->set_map_size_soft(size/num_shards);
  }
}

void TWStringStatsDBWrapper::set_expire_sleep(unsigned int ms)
{
  for (const auto& sdbp : *sdbvp) {
    sdbp->set_expire_sleep(ms);
  }
}

unsigned int TWStringStatsDBWrapper::get_max_size()
{
  unsigned int max_size=0;
  for (const auto& sdbp : *sdbvp) {
    max_size += sdbp->get_max_size();
  }
  return max_size;
}
    
int TWStringStatsDBWrapper::windowSize()
{
  return window_size;
}

int TWStringStatsDBWrapper::numWindows()
{
  return num_windows;
}

int TWStringStatsDBWrapper::numShards()
{
  return num_shards;
}

VTWPtr::const_iterator TWStringStatsDBWrapper::begin() const
{
  return sdbvp->cbegin();
}

VTWPtr::const_iterator TWStringStatsDBWrapper::end() const
{
  return sdbvp->cend();
}

std::list<std::string>::const_iterator TWStringStatsDBWrapper::startDBDump(VTWPtr::const_iterator& sdbp) const
{
  return (*sdbp)->startDBDump();
}

bool TWStringStatsDBWrapper::DBDumpEntry(VTWPtr::const_iterator& sdbp,
                                         std::list<std::string>::const_iterator& i,
                                         TWStatsDBDumpEntry& entry,
                                         std::string& key) const
{
  return (*sdbp)->DBDumpEntry(i, entry, key);
}

bool TWStringStatsDBWrapper::DBGetEntry(VTWPtr::const_iterator& sdbp,
                                        std::list<std::string>::const_iterator& i,
                                        TWStatsDBEntry& entry,
                                        std::string& key) const
{
  return (*sdbp)->DBGetEntry(i, entry, key);
}

const std::list<std::string>::const_iterator TWStringStatsDBWrapper::DBDumpIteratorEnd(VTWPtr::const_iterator& sdbp) const
{
  return (*sdbp)->DBDumpIteratorEnd();
}

void TWStringStatsDBWrapper::endDBDump(VTWPtr::const_iterator& sdbp) const
{
  (*sdbp)->endDBDump();
}

void TWStringStatsDBWrapper::restoreEntry(const std::string& key, std::map<std::string, std::pair<std::time_t, TWStatsBufSerial>>& entry)
{
  std::shared_ptr<TWStatsDB<std::string>> sdbp = sdbvp->at(getShardIndex(key));
  sdbp->restoreEntry(key, entry);
}

void TWStringStatsDBWrapper::startExpireThread()
{
  for (auto& sdbp : *sdbvp) {
    thread t(TWStatsDB<std::string>::twExpireThread, sdbp);
    t.detach();
  }
}
