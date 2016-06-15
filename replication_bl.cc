#include "replication_bl.hh"

BLReplicationOperation::BLReplicationOperation()
{
  op_type = BL_NONE;
}

BLReplicationOperation::BLReplicationOperation(BLOpType my_op_type, BLType my_bl_type, const std::string& my_key, time_t my_ttl, const std::string& my_reason)
{
  op_type = my_op_type;
  type = my_bl_type;
  key = my_key;
  ttl = my_ttl;
  reason = my_reason;
}

Json::object BLReplicationOperation::jsonize()
{
  Json::object jobj;

  jobj.insert(make_pair("op_type", op_type));
  jobj.insert(make_pair("type", type));
  jobj.insert(make_pair("key", key));
  if (op_type == BL_ADD) {
    jobj.insert(make_pair("ttl", (double)ttl));
    jobj.insert(make_pair("reason", reason));
  }
  return jobj;
}

std::shared_ptr<AnyReplicationOperation> BLReplicationOperation::unjsonize(const Json::object& jobj, bool& retval)
{
  retval = true;

  auto it = jobj.find("op_type");
  if (it != jobj.end())
    op_type = (BLOpType)it->second.int_value();
  else
    retval = false;
  if (op_type != BL_NONE) {
    if (retval) {
      it = jobj.find("type");
      if (it!=jobj.end())
	type = (BLType)it->second.int_value();
      else
	retval = false;
    }
    if (retval) {
      it = jobj.find("key");
      if (it!=jobj.end())
	key = it->second.string_value();
      else
	retval = false;
    }
    if (retval) {
      if (op_type == BL_ADD) {
	it = jobj.find("ttl");
	if (it!= jobj.end())
	  ttl = (time_t)it->second.number_value();
	else
	  retval = false;
	it = jobj.find("reason");
	if (it!= jobj.end())
	  reason = it->second.string_value();
	else
	  retval = false;
      }
    }
  }
  return std::make_shared<BLReplicationOperation>(op_type, type, key, ttl, reason);
}

void BLReplicationOperation::applyOperation()
{
  if (op_type != BL_NONE) {
    switch (op_type) {
    case BL_ADD:
      bl_db.addEntryInternal(key, ttl, type, reason, false);
      break;
    case BL_DELETE:
      bl_db.deleteEntryInternal(key, type, false);
      break;
    default:
      errlog("applyOperation: invalid blacklist operation type found");
    }
  }
}
