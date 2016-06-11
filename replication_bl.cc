#include "replication_bl.hh"

BLReplicationOperation::BLReplicationOperation()
{
  op_type = BL_NONE;
}

BLReplicationOperation::BLReplicationOperation(BLOpType my_op_type, const std::string& my_login, time_t my_ttl, const std::string& my_reason)
{
  op_type = my_op_type;
  type = LOGIN_BL;
  login = my_login;
  ttl = my_ttl;
  reason = my_reason;
}

BLReplicationOperation::BLReplicationOperation(BLOpType my_op_type, const ComboAddress& my_ca, time_t my_ttl, const std::string& my_reason)
{
  op_type = my_op_type;
  type = IP_BL;
  ca = my_ca;
  ttl = my_ttl;
  reason = my_reason;
}


BLReplicationOperation::BLReplicationOperation(BLOpType my_op_type, const ComboAddress& my_ca, const std::string& my_login, time_t my_ttl, const std::string& my_reason)
{
  op_type = my_op_type;
  type = IP_LOGIN_BL;
  ca = my_ca;
  login = my_login;
  ttl = my_ttl;
  reason = my_reason;
}

Json::object BLReplicationOperation::jsonize()
{
  Json::object jobj;

  jobj.insert(make_pair("op_type", op_type));
  jobj.insert(make_pair("type", type));
  if ((type == IP_BL) || (type == IP_LOGIN_BL))
    jobj.insert(make_pair("ip", ca.toString()));
  if ((type == LOGIN_BL) || (type == IP_LOGIN_BL))
    jobj.insert(make_pair("login", login));
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
      if ((type == IP_BL) || (type == IP_LOGIN_BL)) {
	it = jobj.find("ip");
	if (it!=jobj.end())
	  ca = ComboAddress(it->second.string_value());
	else
	  retval = false;
      }
    }
    if (retval) {
      if ((type == LOGIN_BL) || (type == IP_LOGIN_BL)) {
	it = jobj.find("login");
	if (it!=jobj.end())
	  login = it->second.string_value();
	else
	  retval = false;
      }
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
  if (type == IP_BL)
    return std::make_shared<BLReplicationOperation>(op_type, ca, ttl, reason);
  else if (type == LOGIN_BL)
    return std::make_shared<BLReplicationOperation>(op_type, login, ttl, reason);
  else if (type == IP_LOGIN_BL)
    return std::make_shared<BLReplicationOperation>(op_type, ca, login, ttl, reason);
  else
    return std::make_shared<BLReplicationOperation>();
}

void BLReplicationOperation::applyOperation()
{
  if (op_type != BL_NONE) {
    switch (type) {
    case IP_BL:
      if (op_type == BL_ADD)
	bl_db.addEntry(ca, ttl, reason);
      else if (op_type == BL_DELETE)
	bl_db.deleteEntry(ca);
      break;
    case LOGIN_BL:
      if (op_type == BL_ADD)
	bl_db.addEntry(login, ttl, reason);
      else if (op_type == BL_DELETE)
	bl_db.deleteEntry(login);
      break;
    case IP_LOGIN_BL:
      if (op_type == BL_ADD)
	bl_db.addEntry(ca, login, ttl, reason);
      else if (op_type == BL_DELETE)
	bl_db.deleteEntry(ca, login);
      break;
    default:
      errlog("applyOperation: invalid blacklist type found");
    }
  }
}
