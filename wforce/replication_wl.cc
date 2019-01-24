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

#include "replication_wl.hh"

std::shared_ptr<AnyReplicationOperation> WLReplicationOperation::unserialize(const std::string& str, bool& retval)
{
  retval = true;

  if (bl_msg.ParseFromString(str)) {
    BLWLType type = static_cast<BLWLType>(bl_msg.type());
    return std::make_shared<WLReplicationOperation>(bl_msg.op_type(), type, bl_msg.key(), bl_msg.ttl(), bl_msg.reason());
  }
  retval = false;
  return std::make_shared<BLReplicationOperation>();
}

void WLReplicationOperation::applyOperation()
{
  if (bl_msg.op_type() != BLOperation_BLOpType_BLNone) {
    BLWLType type = static_cast<BLWLType>(bl_msg.type());
    switch (bl_msg.op_type()) {
    case BLOperation_BLOpType_BLAdd:
      g_wl_db.addEntryInternal(bl_msg.key(), bl_msg.ttl(), type, bl_msg.reason(), false);
      break;
    case BLOperation_BLOpType_BLDelete:
      g_wl_db.deleteEntryInternal(bl_msg.key(), type, false);
      break;
    default:
      errlog("applyOperation: invalid whitelist operation type found");
    }
  }
}
