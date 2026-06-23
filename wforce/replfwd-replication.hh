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
#include "wforce-replication.hh"

class ReplfwdReplication : public WforceReplication {
public:
  bool checkConnFromSibling(const ComboAddress& remote,
                            shared_ptr<Sibling>& recv_sibling);
  void startReplicationWorkerThreads();
  GlobalStateHolder<vector<shared_ptr<Sibling>>>& getReplForwarders() { return d_repl_forwarders; }
  GlobalStateHolder<vector<shared_ptr<Sibling>>>& getReplForwardersSrc() { return d_repl_forwarders_src; }
private:
  GlobalStateHolder<std::vector<std::shared_ptr<Sibling>>> d_repl_forwarders;
  GlobalStateHolder<std::vector<std::shared_ptr<Sibling>>> d_repl_forwarders_src;
};
