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

#pragma once

#include "sholder.hh"
#include "iputils.hh"
#include "sodcrypto.hh"
#include "wforce-sibling.hh"

class WforceReplication {
public:
  WforceReplication() {
    d_sodnonce.init();
  }
  virtual ~WforceReplication() = default;

  void receiveReplicationOperationsTCP(const ComboAddress& local);
  void receiveReplicationOperations(const ComboAddress& local);

  virtual void startReplicationWorkerThreads();
  void encryptMsg(const std::string& msg, std::string& packet);
  void encryptMsgWithKey(const std::string& msg, std::string& packet, const std::string& key, SodiumNonce& nonce,
                         std::mutex& mutex);
  bool decryptMsg(const char* buf, size_t len, std::string& msg);
  void setMaxSiblingRecvQueueSize(size_t size);
  void setNumSiblingThreads(unsigned int num_threads) { d_num_sibling_threads = num_threads; }
  GlobalStateHolder<vector<shared_ptr<Sibling>>>& getSiblings() { return d_siblings; }
  void replicateOperation(const ReplicationOperation& rep_op);
  void setEncryptionKey(const std::string& key) { d_key = key; }
  std::string getEncryptionKey() { return d_key; }
protected:
  virtual bool checkConnFromSibling(const ComboAddress& remote, shared_ptr<Sibling>& recv_sibling);
  void parseTCPReplication(std::shared_ptr<Socket> sockp, const ComboAddress& remote, std::shared_ptr<Sibling> recv_sibling);
  void parseReceivedReplicationMsg(const std::string& msg, const ComboAddress& remote, std::shared_ptr<Sibling> recv_sibling);
  struct SiblingQueueItem {
    std::string msg;
    ComboAddress remote;
    std::shared_ptr<Sibling> recv_sibling;
  };
  GlobalStateHolder<vector<shared_ptr<Sibling>>> d_siblings;
  SodiumNonce d_sodnonce;
  std::string d_key; // The default key to use if no per-sibling key
  std::mutex d_sod_mutx;
  std::mutex d_sibling_queue_mutex;
  std::queue<SiblingQueueItem> d_sibling_queue;
  std::condition_variable d_sibling_queue_cv;
  size_t d_max_sibling_queue_size = 5000;
  unsigned int d_num_sibling_threads = 2;
};