/*
 * This file is part of PowerDNS or weakforced.
 * Copyright -- PowerDNS.COM B.V. and its contributors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 3 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * In addition, for the avoidance of any doubt, permission is granted to
 * link this program with OpenSSL and to (re)distribute the binaries
 * produced as the result of such linking.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "config.h"
#include "ext/threadname.hh"
#include "replication.hh"
#include "replfwd-replication.hh"
#include "wforce_ns.hh"
#include "wforce-prometheus.hh"

using std::atomic;
using std::thread;

bool ReplfwdReplication::checkConnFromSibling(const ComboAddress& remote,
                                              shared_ptr<Sibling>& recv_sibling)
{
  auto siblings = d_siblings.getLocal();

  for (auto &s : *siblings) {
    if (ComboAddress::addressOnlyEqual()(s->rem, remote) == true) {
      recv_sibling = s;
      break;
    }
  }

  if (recv_sibling == nullptr) {
    auto repl_fwd_srcs = d_repl_forwarders_src.getLocal();
    for (auto &f : *repl_fwd_srcs) {
      if (ComboAddress::addressOnlyEqual()(f->rem, remote) == true) {
        recv_sibling = f;
        break;
      }
    }
  }
  
  if (recv_sibling == nullptr) {
    errlog("Message received from host (%s) that is not configured as a sibling or replication forwarder src", remote.toStringWithPort());
    return false;
  }
  return true;
}

void ReplfwdReplication::startReplicationWorkerThreads()
{
  // Tell prometheus how to get the recv queue size
  setPrometheusReplRecvQueueRetrieveFunc([this]() -> int {
      std::lock_guard<std::mutex> lock(d_sibling_queue_mutex);
      return d_sibling_queue.size();
    });
  for (size_t i=0; i<d_num_sibling_threads; i++) {
    std::thread t([this]() {
        setThreadName("rf/repl-worker");
        while (true) {
          SiblingQueueItem sqi;
          {
            std::unique_lock<std::mutex> lock(d_sibling_queue_mutex);
            while (d_sibling_queue.empty()) {
              d_sibling_queue_cv.wait(lock);
            }
            sqi = std::move(d_sibling_queue.front());
            d_sibling_queue.pop();
          }
          ReplicationOperation rep_op;
          if (rep_op.unserialize(sqi.msg)) {
            sqi.recv_sibling->rcvd_success++;
            // note no port because it can be ephemeral
            // (thus rcvd stats are tracked only on a per-IP basis)
            incPrometheusReplicationRcvd(sqi.remote.toString(), true);

            // Now we decide whether to forward the replication event
            if (rep_op.getForwarded()) {
              // This means we have a forwarded message, which we need to send to our siblings
              // First we remove the forwarded flag
              rep_op.setForwarded(false);
              // Now replicate it to our siblings
              debuglog("Forwarding message from forwarder to siblings");
              replicateOperation(rep_op);
            }
            else {
              // This means we have a message from our siblings, which we need to decide
              // whether to forward
              auto fwdrs = d_repl_forwarders.getLocal();
              std::string serialized_op;
              // We can't just forward the encrypted message
              // First we have to set the forwarded flag
              rep_op.setForwarded(true);
              serialized_op = rep_op.serialize();
              for (auto &s : *fwdrs) {
                if ((s->sdb_send && (rep_op.getType() == WforceReplicationMsg_RepType_SDBType)) ||
                    (s->wlbl_send && ((rep_op.getType() == WforceReplicationMsg_RepType_BlacklistType) ||
                                   (rep_op.getType() == WforceReplicationMsg_RepType_WhitelistType)))) {
                  string packet;
                  // Then encrypt
                  if (s->d_has_key) {
                    if (s->d_key != d_key) {
                      encryptMsgWithKey(serialized_op, packet, s->d_key, s->d_nonce, s->mutx);
                    }
                  }
                  else { // No per-sibling key - use the global one
                    encryptMsg(serialized_op, packet);
                  }
                  // Finally we can send the encrypted message to the forward destination
                  debuglog("Forwarding message from sibling to forwarder");
                  s->queueMsg(packet);
                }
              }
            }
          }
          else {
            errlog("Invalid replication operation received from %s", sqi.remote.toString());
            sqi.recv_sibling->rcvd_fail++;
            // note no port because it can be ephemeral
            // (thus rcvd stats are tracked only on a per-IP basis)
            incPrometheusReplicationRcvd(sqi.remote.toString(), false);
          }
        }
      });
    t.detach();
  }
}

