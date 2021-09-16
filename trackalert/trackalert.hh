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
#include <mutex>
#include <thread>
#include "sholder.hh"
#include "sstuff.hh"
#include "webhook.hh"
#include "wforce-webserver.hh"
#include "wforce_exception.hh"
#include "login_tuple.hh"
#include "json11.hpp"
#include "ext/scheduler/Scheduler.h"
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/identity.hpp>
#include <boost/multi_index/sequenced_index.hpp>
struct TrackalertStats
{
  using stat_t=std::atomic<uint64_t>;
  stat_t reports{0};
};

extern struct TrackalertStats g_stats;

struct ClientState
{
  ComboAddress local;
  int udpFD;
  int tcpFD;
};

extern std::mutex g_luamutex;
extern LuaContext g_lua;
extern std::string g_outputBuffer; // locking for this is ok, as locked by g_luamutex (functions using g_outputBuffer MUST NOT be enabled for the allow/report lua contexts)
extern WforceWebserver g_webserver;

void receiveReports(ComboAddress local);

extern GlobalStateHolder<NetmaskGroup> g_ACL;
extern ComboAddress g_serverControl; // not changed during runtime

extern std::string g_key; // in theory needs locking

void controlThread(int fd, ComboAddress local);
bool getMsgLen(int fd, uint16_t* len);
bool putMsgLen(int fd, uint16_t len);
void* tcpAcceptorThread(void* p);
double getDoubleTime();

extern WebHookRunner g_webhook_runner;
extern WebHookDB g_webhook_db;
extern WebHookDB g_custom_webhook_db;

extern bool g_configurationDone;

#define NUM_SCHEDULER_THREADS 4
extern int g_num_scheduler_threads;
extern std::shared_ptr<Bosma::Scheduler> g_bg_schedulerp;
