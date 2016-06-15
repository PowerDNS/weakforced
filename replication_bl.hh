#pragma once
#include "replication.hh"
#include "blacklist.hh"

enum BLOpType { BL_ADD=0, BL_DELETE=1, BL_NONE=999 };

class BLReplicationOperation : public AnyReplicationOperation
{
public:
  BLReplicationOperation();
  BLReplicationOperation(BLOpType op_type, BLType bl_type, const std::string& key, time_t ttl, const std::string& reason);
  Json::object jsonize();
  AnyReplicationOperationP unjsonize(const Json::object& jobj, bool& retval);
  void applyOperation();
private:
  BLOpType op_type;
  BLType type;
  std::string key;
  time_t ttl;
  std::string reason;
};
