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
