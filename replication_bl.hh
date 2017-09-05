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
#include "replication.hh"
#include "replication.pb.h"
#include "blacklist.hh"

class BLReplicationOperation : public AnyReplicationOperation
{
public:
  BLReplicationOperation();
  BLReplicationOperation(BLOperation_BLOpType op_type, BLType bl_type, const std::string& key, time_t ttl, const std::string& reason);
  std::string serialize();
  AnyReplicationOperationP unserialize(const std::string& str, bool& retval);
  void applyOperation();
private:
  BLOperation bl_msg;
};
