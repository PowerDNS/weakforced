/*
 * This file is part of PowerDNS or dnsdist.
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

#include "twmap-wrapper.hh"
#include "replication.hh"
#include "replication_sdb.hh"
#include "replication.pb.h"
#include "wforce.hh"

TWStringStatsDBWrapper::TWStringStatsDBWrapper(const std::string& name, int window_size, int num_windows)
{
  sdbp = std::make_shared<TWStatsDB<std::string>>(name, window_size, num_windows);
  replicated = std::make_shared<bool>(false);
  thread t(TWStatsDB<std::string>::twExpireThread, sdbp);
  t.detach();
}

TWStringStatsDBWrapper::TWStringStatsDBWrapper(const std::string& name, int window_size, int num_windows, const std::vector<pair<std::string, std::string>>& fmvec)
{
  sdbp = std::make_shared<TWStatsDB<std::string>>(name, window_size, num_windows);    
  replicated = std::make_shared<bool>(false);
  (void)setFields(fmvec);
  thread t(TWStatsDB<std::string>::twExpireThread, sdbp);
  t.detach();
}

void TWStringStatsDBWrapper::enableReplication()
{
  *replicated = true;
}

void TWStringStatsDBWrapper::disableReplication()
{
  *replicated = false;
}

bool TWStringStatsDBWrapper::setFields(const std::vector<pair<std::string, std::string>>& fmvec)
{
  FieldMap fm;
  for(const auto& f : fmvec) {
    fm.insert(std::make_pair(f.first, f.second));
  }
  return sdbp->setFields(fm);
}

bool TWStringStatsDBWrapper::setFields(const FieldMap& fm) 
{
  return(sdbp->setFields(fm));
}

void TWStringStatsDBWrapper::setv4Prefix(uint8_t bits)
{
  uint8_t v4_prefix = bits > 32 ? 32 : bits;
  sdbp->setv4Prefix(v4_prefix);
}

void TWStringStatsDBWrapper::setv6Prefix(uint8_t bits)
{
  uint8_t v6_prefix = bits > 128 ? 128 : bits;
  sdbp->setv6Prefix(v6_prefix);
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
      uint8_t v4_prefix = sdbp->getv4Prefix();
      return Netmask(ca, v4_prefix).toStringNetwork();
    }
    else if (ca.isIpv6()) {
      uint8_t v6_prefix = sdbp->getv6Prefix();
      return Netmask(ca, v6_prefix).toStringNetwork();
    }
  }
  return std::string{};
}

void TWStringStatsDBWrapper::add(const TWKeyType vkey, const std::string& field_name, const boost::variant<std::string, int, ComboAddress>& param1, boost::optional<int> param2)
{
  addInternal(vkey, field_name, param1, param2, true);
}

void TWStringStatsDBWrapper::addInternal(const TWKeyType vkey, const std::string& field_name, const boost::variant<std::string, int, ComboAddress>& param1, boost::optional<int> param2, bool replicate)
{	
  std::string key = getStringKey(vkey);
  std::shared_ptr<SDBReplicationOperation> sdb_rop;
  std::string db_name = sdbp->getDBName();
  
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
  std::string db_name = sdbp->getDBName();

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
  std::string db_name = sdbp->getDBName();

  sdbp->reset(key);

  if ((replicate == true) && (*replicated == true)) {
    sdb_rop = std::make_shared<SDBReplicationOperation>(db_name, SDBOperation_SDBOpType_SDBOpReset, key);
    ReplicationOperation rep_op(sdb_rop, WforceReplicationMsg_RepType_SDBType);
    // this actually does the replication
    replicateOperation(rep_op);
  }
}

unsigned int TWStringStatsDBWrapper::get_size()
{	
  return sdbp->get_size();
}

void TWStringStatsDBWrapper::set_size_soft(unsigned int size) 
{
  sdbp->set_map_size_soft(size);
}
