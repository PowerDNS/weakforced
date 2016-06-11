#pragma once
#include "ext/json11/json11.hpp"

using namespace json11;

class AnyReplicationOperation
{
public:
  virtual Json::object jsonize()=0;
  virtual std::shared_ptr<AnyReplicationOperation> unjsonize(const Json::object& jobj, bool& retval)=0;
  virtual void applyOperation()=0;
};

typedef std::shared_ptr<AnyReplicationOperation> AnyReplicationOperationP;

class ReplicationOperation
{
public:
  enum ReplObjectType { REPL_BLACKLIST=0, REPL_STATSDB=1, REPL_NONE=999 };

  ReplicationOperation() : obj_type(REPL_NONE)
  {}
  ReplicationOperation(AnyReplicationOperationP op, ReplObjectType type) : rep_op(op), obj_type(type)
  {
  }
  std::string serialize() const;
  bool unserialize(const std::string& str);
  void applyOperation();

private:
  AnyReplicationOperationP rep_op;
  ReplObjectType obj_type;
};
