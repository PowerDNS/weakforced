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

#include "replication_sdb.hh"

SDBReplicationOperation::SDBReplicationOperation()
{
  sdb_msg.set_op_type(SDBOperation_SDBOpType_SDBOpNone);
}

SDBReplicationOperation::SDBReplicationOperation(const std::string& my_db_name, SDBOperation_SDBOpType my_op, const std::string& my_key)
{
  sdb_msg.set_db_name(my_db_name);
  sdb_msg.set_op_type(my_op);
  sdb_msg.set_key(my_key);
}

SDBReplicationOperation::SDBReplicationOperation(const std::string& my_db_name, SDBOperation_SDBOpType my_op,
						    const std::string& my_key, const std::string& my_field_name,
						    const std::string& my_s) : SDBReplicationOperation(my_db_name, my_op, my_key)

{
  sdb_msg.set_field_name(my_field_name);
  sdb_msg.set_str_param(my_s);
}

SDBReplicationOperation::SDBReplicationOperation(const std::string& my_db_name, SDBOperation_SDBOpType my_op,
						    const std::string& my_key, const std::string& my_field_name,
						    int my_a) : SDBReplicationOperation(my_db_name, my_op, my_key)
{
  sdb_msg.set_field_name(my_field_name);
  sdb_msg.set_int_param(my_a);
}

SDBReplicationOperation::SDBReplicationOperation(const std::string& my_db_name, SDBOperation_SDBOpType my_op,
						    const std::string& my_key, const std::string& my_field_name,
						    const std::string& my_s, int my_a) : SDBReplicationOperation(my_db_name, my_op, my_key)
{
  sdb_msg.set_field_name(my_field_name);
  sdb_msg.set_str_param(my_s);
  sdb_msg.set_int_param(my_a);
}

SDBReplicationOperation::SDBReplicationOperation(const std::string& my_db_name,
                                                 SDBOperation_SDBOpType my_op,
                                                 const std::string& my_key,
                                                 const std::string& my_field_name) : SDBReplicationOperation(my_db_name, my_op, my_key)
{
  sdb_msg.set_field_name(my_field_name);
}

std::string SDBReplicationOperation::serialize()
{
  std::string ret_str;
  sdb_msg.SerializeToString(&ret_str);
  return ret_str;
}

AnyReplicationOperationP SDBReplicationOperation::unserialize(const std::string& str, bool& retval)
{
  retval = true;

  if (sdb_msg.ParseFromString(str)) {
    if ((sdb_msg.has_field_name() == false) && (sdb_msg.has_str_param() == false) && (sdb_msg.has_int_param() == false))
      return std::make_shared<SDBReplicationOperation>(sdb_msg.db_name(), sdb_msg.op_type(), sdb_msg.key());
    else if ((sdb_msg.has_field_name() == true) && (sdb_msg.has_str_param() == true) && (sdb_msg.has_int_param() == false))
      return std::make_shared<SDBReplicationOperation>(sdb_msg.db_name(), sdb_msg.op_type(), sdb_msg.key(), sdb_msg.field_name(), sdb_msg.str_param());
    else if ((sdb_msg.has_field_name() == true) && (sdb_msg.has_str_param() == false) && (sdb_msg.has_int_param() == true))
      return std::make_shared<SDBReplicationOperation>(sdb_msg.db_name(), sdb_msg.op_type(), sdb_msg.key(), sdb_msg.field_name(), sdb_msg.int_param());
    else if ((sdb_msg.has_field_name() == true) && (sdb_msg.has_str_param() == true) && (sdb_msg.has_int_param() == true))
      return std::make_shared<SDBReplicationOperation>(sdb_msg.db_name(), sdb_msg.op_type(), sdb_msg.key(), sdb_msg.field_name(), sdb_msg.str_param(), sdb_msg.int_param());
    else if ((sdb_msg.has_field_name() == true) && (sdb_msg.has_str_param() == false) && (sdb_msg.has_int_param() == false))
      return std::make_shared<SDBReplicationOperation>(sdb_msg.db_name(), sdb_msg.op_type(), sdb_msg.key(), sdb_msg.field_name());
  }

  retval = false;
  return std::make_shared<SDBReplicationOperation>();    
}

void SDBReplicationOperation::applyOperation()
{
  if (sdb_msg.op_type() != SDBOperation_SDBOpType_SDBOpNone) {
    TWStringStatsDBWrapper* statsdb;
    {
      std::lock_guard<std::mutex> lock(dbMap_mutx);
      auto it = dbMap.find(sdb_msg.db_name());
      if (it != dbMap.end())
	statsdb = &(it->second);
      else {
	errlog("applyOperation(): Could not find db %s", sdb_msg.db_name());
	return;
      }
    }
    switch(sdb_msg.op_type()) {
    case SDBOperation_SDBOpType_SDBOpReset:
      statsdb->resetInternal(sdb_msg.key(), false);
      break;
    case SDBOperation_SDBOpType_SDBOpResetField:
      statsdb->resetFieldInternal(sdb_msg.key(), sdb_msg.field_name(), false);
      break;
    case SDBOperation_SDBOpType_SDBOpAdd:
      if (sdb_msg.has_field_name() == false) {
	errlog("applyOperation(): No field_name found for statsdb add");
	return;
      }
      if ((sdb_msg.has_str_param() == true) && (sdb_msg.has_int_param() == true))
	statsdb->addInternal(sdb_msg.key(), sdb_msg.field_name(), sdb_msg.str_param(), sdb_msg.int_param(), false);
      else if (sdb_msg.has_str_param() == true)
	statsdb->addInternal(sdb_msg.key(), sdb_msg.field_name(), sdb_msg.str_param(), {}, false);
      else if (sdb_msg.has_int_param() == true)
	statsdb->addInternal(sdb_msg.key(), sdb_msg.field_name(), sdb_msg.int_param(), {}, false);
      else
	errlog("applyOperation(): Malformed statsdb add");
      break;
    case SDBOperation_SDBOpType_SDBOpSub:
      if (sdb_msg.has_field_name() == false) {
	errlog("applyOperation(): No field_name found for statsdb sub");
	return;
      }
      if (sdb_msg.has_str_param() == true)
	statsdb->subInternal(sdb_msg.key(), sdb_msg.field_name(), sdb_msg.str_param(), false);
      else if (sdb_msg.has_int_param() == true)
	statsdb->subInternal(sdb_msg.key(), sdb_msg.field_name(), sdb_msg.int_param(), false);
      else
	errlog("applyOperation(): Malformed statsdb sub");
      break;
    default:
      errlog("applyOperation(): Invalid operation %d", sdb_msg.op_type());
    }
  }
}
