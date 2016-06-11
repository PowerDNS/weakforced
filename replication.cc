#include "replication.hh"
#include "replication_sdb.hh"
#include "replication_bl.hh"

std::string ReplicationOperation::serialize() const
{
  Json::object jobj = rep_op->jsonize();
  Json msg = Json::object {
    {"obj_type", obj_type},
    {"rep_op", jobj} };
  return msg.dump();
}

bool ReplicationOperation::unserialize(const std::string& str)
{
  string err;
  json11::Json msg=json11::Json::parse(str, err);
  bool retval=false;
  
  obj_type = (ReplObjectType)msg["obj_type"].int_value();
  Json::object jobj = msg["rep_op"].object_items();
  if (obj_type == REPL_BLACKLIST) {
    BLReplicationOperation bl_op = BLReplicationOperation();
    rep_op = bl_op.unjsonize(jobj, retval);
  }
  else if (obj_type == REPL_STATSDB) {
    SDBReplicationOperation<std::string> sdb_op = SDBReplicationOperation<std::string>();
    rep_op = sdb_op.unjsonize(jobj, retval);
  }
  return retval;
}

void ReplicationOperation::applyOperation()
{
  rep_op->applyOperation();
}
