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
#include "ext/luawrapper/include/LuaContext.hpp"
#include <time.h>
#include "misc.hh"
#include "iputils.hh"
#include <atomic>
#include <boost/circular_buffer.hpp>
#include <mutex>
#include <thread>
#include "sholder.hh"
#include "sstuff.hh"
#include "webhook.hh"
#include "replication.hh"
#include "replfwd-replication.hh"
#include "wforce-webserver.hh"
#include "wforce_exception.hh"
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/identity.hpp>
#include <boost/multi_index/sequenced_index.hpp>

struct WForceStats
{
  using stat_t=std::atomic<uint64_t>;
  stat_t reports{0};
  stat_t allows{0};
  stat_t denieds{0};
  double latency{0};
  
};

extern struct WForceStats g_stats;

extern std::mutex g_luamutex;
extern LuaContext g_lua;
extern std::string g_outputBuffer; // locking for this is ok, as locked by g_luamutex (functions using g_outputBuffer MUST NOT be enabled for the allow/report lua contexts)
extern WforceWebserver g_webserver;

extern GlobalStateHolder<NetmaskGroup> g_ACL;

extern WebHookRunner g_webhook_runner;
extern WebHookDB g_webhook_db;

extern ReplfwdReplication g_replication;

vector<std::function<void(void)>> setupLua(LuaContext& c_lua,
					   const std::string& config);
