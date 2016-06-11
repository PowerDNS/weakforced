#pragma once
#include "replication.hh"
#include "blacklist.hh"

class BLReplicationOperation : public AnyReplicationOperation
{
public:
  enum BLOpType { BL_ADD=0, BL_DELETE=1, BL_NONE=999 };
  BLReplicationOperation();
  BLReplicationOperation(BLOpType op_type, const std::string& login, time_t ttl, const std::string& reason);
  BLReplicationOperation(BLOpType op_type, const ComboAddress& ca, time_t ttl, const std::string& reason);
  BLReplicationOperation(BLOpType op_type, const ComboAddress& ca, const std::string& login, time_t ttl, const std::string& reason);
  Json::object jsonize();
  AnyReplicationOperationP unjsonize(const Json::object& jobj, bool& retval);
  void applyOperation();
private:
  BLOpType op_type;
  BLType type;
  std::string login;
  ComboAddress ca;
  time_t ttl;
  std::string reason;
};
