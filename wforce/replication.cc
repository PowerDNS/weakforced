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

#include "replication.hh"
#include "replication_sdb.hh"
#include "replication_bl.hh"
#include "replication_wl.hh"

std::string ReplicationOperation::serialize() const
{
  std::string bytes = rep_op->serialize();
  WforceReplicationMsg msg;

  msg.set_rep_type(obj_type);
  msg.set_rep_op(bytes);

  std::string ret_str;
  msg.SerializeToString(&ret_str);
  return ret_str;
}

bool ReplicationOperation::unserialize(const std::string& str)
{
  string err;
  bool retval=true;
  WforceReplicationMsg msg;
  
  if (msg.ParseFromString(str)) {
    obj_type = msg.rep_type();
    if (obj_type == WforceReplicationMsg_RepType_SDBType) {
      SDBReplicationOperation sdb_op = SDBReplicationOperation();
      rep_op = sdb_op.unserialize(msg.rep_op(), retval);      
    }
    else if (obj_type == WforceReplicationMsg_RepType_BlacklistType) {
      BLReplicationOperation bl_op = BLReplicationOperation();
      rep_op = bl_op.unserialize(msg.rep_op(), retval);
    }
    else if (obj_type == WforceReplicationMsg_RepType_WhitelistType) {
      WLReplicationOperation wl_op = WLReplicationOperation();
      rep_op = wl_op.unserialize(msg.rep_op(), retval);
    }
    else
      retval = false;
  }  
  return retval;
}

void ReplicationOperation::applyOperation()
{
  rep_op->applyOperation();
}
