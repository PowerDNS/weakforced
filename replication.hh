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
