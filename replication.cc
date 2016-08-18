#include "replication.hh"
#include "replication_sdb.hh"
#include "replication_bl.hh"

std::string ReplicationOperation::serialize() const
{
  std::string bytes = rep_op->serialize();
  WforceReplicationMsg msg;

  msg.set_rep_type(obj_type);
  msg.set_rep_op(bytes);

  std::string ret_str;
  msg.SerializeToString(&ret_str);
  return ret_str;
}

bool ReplicationOperation::unserialize(const std::string& str)
{
  string err;
  bool retval=false;
  WforceReplicationMsg msg;
  
  if (msg.ParseFromString(str)) {
    obj_type = msg.rep_type();
    if (obj_type == WforceReplicationMsg_RepType_SDBType) {
      SDBReplicationOperation sdb_op = SDBReplicationOperation();
      rep_op = sdb_op.unserialize(msg.rep_op(), retval);      
    }
    else if (obj_type == WforceReplicationMsg_RepType_BlacklistType) {
      BLReplicationOperation bl_op = BLReplicationOperation();
      rep_op = bl_op.unserialize(msg.rep_op(), retval);
    }
  }  
  return retval;
}

void ReplicationOperation::applyOperation()
{
  rep_op->applyOperation();
}
