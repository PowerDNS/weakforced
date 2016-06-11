#pragma once
#include "wforce.hh"
#include "blacklist.hh"
#include "twmap.hh"
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

template <typename T>
class SDBReplicationOperation : public AnyReplicationOperation
{
public:
  enum RepOpType { REP_OP_ADD=0, REP_OP_SUB=1, REP_OP_RESET=2, REP_OP_NONE=999 };
  SDBReplicationOperation();
  SDBReplicationOperation(const std::string& db_name, RepOpType op, const T& key);
  SDBReplicationOperation(const std::string& db_name, RepOpType op, const T& key,
			  const std::string& field_name, const std::string& s);
  SDBReplicationOperation(const std::string& db_name, RepOpType op, const T& key,
			  const std::string& field_name, int a);
  SDBReplicationOperation(const std::string& db_name, RepOpType op, const T& key,
			  const std::string& field_name, const std::string& s, int a);
  Json::object jsonize();
  AnyReplicationOperationP unjsonize(const Json::object& jobj, bool& retval);
  void applyOperation();
private:
  std::string db_name;
  RepOpType op;
  T key;
  bool field_name_present=false;
  std::string field_name;
  bool str_param_present=false;
  std::string str_param;
  bool int_param_present=false;
  int int_param;
};

template <typename T>
SDBReplicationOperation<T>::SDBReplicationOperation()
{
  op = REP_OP_NONE;
}

template <typename T>
SDBReplicationOperation<T>::SDBReplicationOperation(const std::string& my_db_name, RepOpType my_op, const T& my_key)
{
  db_name = my_db_name;
  op = my_op;
  key = my_key;
}

template <typename T>
SDBReplicationOperation<T>::SDBReplicationOperation(const std::string& my_db_name, RepOpType my_op,
						    const T& my_key, const std::string& my_field_name,
						    const std::string& my_s)
{
  SDBReplicationOperation(my_db_name, my_op, my_key);
  field_name = my_field_name;
  field_name_present = true;
  str_param = my_s;
  str_param_present = true;
}

template <typename T>
SDBReplicationOperation<T>::SDBReplicationOperation(const std::string& my_db_name, RepOpType my_op,
						    const T& my_key, const std::string& my_field_name,
						    int my_a)
{
  SDBReplicationOperation(my_db_name, my_op, my_key);
  field_name = my_field_name;
  field_name_present = true;
  int_param = my_a;
  int_param_present = true;
}

template <typename T>
SDBReplicationOperation<T>::SDBReplicationOperation(const std::string& my_db_name, RepOpType my_op,
						    const T& my_key, const std::string& my_field_name,
						    const std::string& my_s, int my_a)
{
  SDBReplicationOperation(my_db_name, my_op, my_key);
  field_name = my_field_name;
  field_name_present = true;
  str_param = my_s;
  str_param_present = true;
  int_param = my_a;
  int_param_present = true;
}


template <typename T>
Json::object SDBReplicationOperation<T>::jsonize()
{
  Json::object jobj;

  jobj.insert(make_pair("db_name", db_name));
  jobj.insert(make_pair("op", op));
  jobj.insert(make_pair("key", key));
  if (field_name_present == true)
    jobj.insert(make_pair("field_name", field_name));
  if (str_param_present == true)
    jobj.insert(make_pair("str_param", str_param));
  if (int_param_present == true)
    jobj.insert(make_pair("int_param", int_param));

  return jobj;
}

template <typename T>
AnyReplicationOperationP SDBReplicationOperation<T>::unjsonize(const Json::object& jobj, bool& retval)
{
  retval = true;
  // we need a minimum of db_name, op and key
  auto it = jobj.find("db_name");
  if (it != jobj.end())
    db_name = it->second.string_value();
  else
    retval = false;
  if (retval) {
    it = jobj.find("op");
    if (it!=jobj.end())
      op = (RepOpType)it->second.int_value();
    else
      retval = false;
  }
  if (retval) {
    it = jobj.find("key");
    if (it!= jobj.end())
      key = it->second.string_value();
    else
      retval = false;
  }
  if (retval) {
    it = jobj.find("field_name");
    if (it!=jobj.end()) {
      field_name_present = true;
      field_name = it->second.string_value();
    }
    it = jobj.find("str_param");
    if (it!=jobj.end()) {
      str_param_present = true;
      str_param = it->second.string_value();
    }
    it = jobj.find("int_param");
    if (it!=jobj.end()) {
      int_param = true;
      int_param = it->second.int_value();
    }
  }
  if ((field_name_present == false) && (str_param_present == false) && (int_param_present == false))
    return std::make_shared<SDBReplicationOperation>(db_name, op, key);
  else if ((field_name_present == true) && (str_param_present == true) && (int_param_present == false))
    return std::make_shared<SDBReplicationOperation>(db_name, op, key, field_name, str_param);
  else if ((field_name_present == true) && (str_param_present == false) && (int_param_present == true))
    return std::make_shared<SDBReplicationOperation>(db_name, op, key, field_name, int_param);
  else if ((field_name_present == true) && (str_param_present == true) && (int_param_present == true))
    return std::make_shared<SDBReplicationOperation>(db_name, op, key, field_name, str_param, int_param);
  else
    return std::make_shared<SDBReplicationOperation>();    
}

template <typename T>
void SDBReplicationOperation<T>::applyOperation()
{
  if (op != REP_OP_NONE) {
    TWStringStatsDBWrapper* statsdb;
    {
      std::lock_guard<std::mutex> lock(dbMap_mutx);
      auto it = dbMap.find(db_name);
      if (it != dbMap.end())
	statsdb = &(it->second);
      else {
	errlog("applyOperation(): Could not find db %s", db_name);
	return;
      }
    }
    switch(op) {
    case REP_OP_RESET:
      statsdb->reset(key);
      break;
    case REP_OP_ADD:
      if (field_name_present == false) {
	errlog("applyOperation(): No field_name found for statsdb add");
	return;
      }
      if ((str_param_present == true) && (int_param_present == true))
	statsdb->add(key, field_name, str_param, int_param);
      else if (str_param_present == true)
	statsdb->add(key, field_name, str_param, {});
      else if (int_param_present == true)
	statsdb->add(key, field_name, int_param, {});
      else
	errlog("applyOperation(): Malformed statsdb add");
      break;
    case REP_OP_SUB:
      if (field_name_present == false) {
	errlog("applyOperation(): No field_name found for statsdb sub");
	return;
      }
      if (str_param_present == true)
	statsdb->sub(key, field_name, str_param);
      else if (int_param_present == true)
	statsdb->sub(key, field_name, int_param);
      else
	errlog("applyOperation(): Malformed statsdb sub");
      break;
    default:
      errlog("applyOperation(): Invalid operation %d", op);
    }
  }
}

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

class ReplicationOperation
{
public:
  enum ReplObjectType { REPL_BLACKLIST=0, REPL_STATSDB=1 };

  ReplicationOperation(AnyReplicationOperationP op, ReplObjectType type) : rep_op(op), obj_type(type)
  {
  }
  std::string serialize();
  void unserialize(const std::string& str);
  void applyOperation();

private:
  AnyReplicationOperationP rep_op;
  ReplObjectType obj_type;
};
