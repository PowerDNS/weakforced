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
#include "twmap-wrapper.hh"
#include "replication.hh"
#include "replication.pb.h"

class SDBReplicationOperation : public AnyReplicationOperation
{
public:
  SDBReplicationOperation();
  SDBReplicationOperation(SDBOperation&& so) {
    sdb_msg = so;
  }
  ~SDBReplicationOperation() {}
  SDBReplicationOperation(const std::string& db_name, SDBOperation_SDBOpType op, const std::string& key);
  SDBReplicationOperation(const std::string& db_name, SDBOperation_SDBOpType op, const std::string& key,
			  const std::string& field_name, const std::string& s);
  SDBReplicationOperation(const std::string& db_name, SDBOperation_SDBOpType op, const std::string& key,
			  const std::string& field_name, int a);
  SDBReplicationOperation(const std::string& db_name, SDBOperation_SDBOpType op, const std::string& key,
			  const std::string& field_name, const std::string& s, int a);
  SDBReplicationOperation(const std::string& db_name,
                          SDBOperation_SDBOpType op,
                          const std::string& key,
			  const std::string& field_name);
  SDBReplicationOperation(const std::string& my_db_name,
                          SDBOperation_SDBOpType my_op,
                          const std::string& my_key,
                          const TWStatsDBDumpEntry& my_entry);
  std::string serialize();
  AnyReplicationOperationP unserialize(const std::string& str, bool& retval);
  void applyOperation();
private:
  SDBOperation sdb_msg;
};
