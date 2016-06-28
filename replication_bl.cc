#include "replication_bl.hh"

BLReplicationOperation::BLReplicationOperation()
{
  bl_msg.set_op_type(BLOperation_BLOpType_BLNone);
}

BLReplicationOperation::BLReplicationOperation(BLOperation_BLOpType my_op_type, BLType my_bl_type, const std::string& my_key, time_t my_ttl, const std::string& my_reason)
{
  bl_msg.set_op_type(my_op_type);
  bl_msg.set_type(my_bl_type);
  bl_msg.set_key(my_key);
  bl_msg.set_ttl(my_ttl);
  bl_msg.set_reason(my_reason);
}

std::string BLReplicationOperation::serialize()
{
  std::string ret_str;

  bl_msg.SerializeToString(&ret_str);
  return ret_str;
}

std::shared_ptr<AnyReplicationOperation> BLReplicationOperation::unserialize(const std::string& str, bool& retval)
{
  retval = true;

  if (bl_msg.ParseFromString(str)) {
    BLType type = static_cast<BLType>(bl_msg.type());
    return std::make_shared<BLReplicationOperation>(bl_msg.op_type(), type, bl_msg.key(), bl_msg.ttl(), bl_msg.reason());
  }
  retval = false;
  return std::make_shared<BLReplicationOperation>();
}

void BLReplicationOperation::applyOperation()
{
  if (bl_msg.op_type() != BLOperation_BLOpType_BLNone) {
    BLType type = static_cast<BLType>(bl_msg.type());
    switch (bl_msg.op_type()) {
    case BLOperation_BLOpType_BLAdd:
      bl_db.addEntryInternal(bl_msg.key(), bl_msg.ttl(), type, bl_msg.reason(), false);
      break;
    case BLOperation_BLOpType_BLDelete:
      bl_db.deleteEntryInternal(bl_msg.key(), type, false);
      break;
    default:
      errlog("applyOperation: invalid blacklist operation type found");
    }
  }
}
