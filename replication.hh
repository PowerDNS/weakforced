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

#pragma once
#include "replication.pb.h"
#include <memory>

class AnyReplicationOperation
{
public:
  virtual std::string serialize()=0;
  virtual std::shared_ptr<AnyReplicationOperation> unserialize(const std::string& data, bool& retval)=0;
  virtual void applyOperation()=0;
};

typedef std::shared_ptr<AnyReplicationOperation> AnyReplicationOperationP;

class ReplicationOperation
{
public:

  ReplicationOperation() : obj_type(WforceReplicationMsg_RepType_NoneType)
  {}
  ReplicationOperation(AnyReplicationOperationP op, WforceReplicationMsg_RepType type) : rep_op(op), obj_type(type)
  {
  }
  std::string serialize() const;
  bool unserialize(const std::string& str);
  void applyOperation();

private:
  AnyReplicationOperationP rep_op;
  WforceReplicationMsg_RepType obj_type;
};
