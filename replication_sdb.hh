#pragma once
#include "twmap-wrapper.hh"
#include "replication.hh"
#include "replication.pb.h"

class SDBReplicationOperation : public AnyReplicationOperation
{
public:
  SDBReplicationOperation();
  SDBReplicationOperation(const std::string& db_name, SDBOperation_SDBOpType op, const std::string& key);
  SDBReplicationOperation(const std::string& db_name, SDBOperation_SDBOpType op, const std::string& key,
			  const std::string& field_name, const std::string& s);
  SDBReplicationOperation(const std::string& db_name, SDBOperation_SDBOpType op, const std::string& key,
			  const std::string& field_name, int a);
  SDBReplicationOperation(const std::string& db_name, SDBOperation_SDBOpType op, const std::string& key,
			  const std::string& field_name, const std::string& s, int a);
  std::string serialize();
  AnyReplicationOperationP unserialize(const std::string& str, bool& retval);
  void applyOperation();
private:
  SDBOperation sdb_msg;
};
