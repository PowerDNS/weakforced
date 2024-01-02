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
#include "replication.hh"
#include "webhook.hh"
#include "wforce-webserver.hh"
#include "wforce_exception.hh"
#include "login_tuple.hh"
#include "json11.hpp"
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/identity.hpp>
#include <boost/multi_index/sequenced_index.hpp>
#include "wforce-sibling.hh"
#include "wforce-replication.hh"

struct WForceStats
{
  using stat_t=std::atomic<uint64_t>;
  stat_t reports{0};
  stat_t allows{0};
  stat_t denieds{0};
  double latency{0};
  
};

extern struct WForceStats g_stats;

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
extern WforceReplication g_replication;

extern GlobalStateHolder<NetmaskGroup> g_ACL;
extern ComboAddress g_sibling_listen;
extern ComboAddress g_serverControl; // not changed during runtime

struct dnsheader;

void controlThread(int fd, ComboAddress local);
bool getMsgLen(int fd, uint16_t* len);
bool putMsgLen(int fd, uint16_t len);
void* tcpAcceptorThread(void* p);
double getDoubleTime();

extern GlobalStateHolder<vector<shared_ptr<Sibling>>> g_report_sinks;
extern GlobalStateHolder<std::map<std::string, std::pair<std::shared_ptr<std::atomic<unsigned int>>, std::vector<std::shared_ptr<Sibling>>>>> g_named_report_sinks;
void sendReportSink(const LoginTuple& lt);
void sendNamedReportSink(const std::string& msg);

extern WebHookRunner g_webhook_runner;
extern WebHookDB g_webhook_db;
extern WebHookDB g_custom_webhook_db;
extern std::shared_ptr<UserAgentParser> g_ua_parser_p;

extern bool g_allowlog_verbose; // Whether to log allow returns of 0

void dumpEntriesThread(const ComboAddress& ca, std::unique_lock<std::mutex> lock);
void syncDBThread(const ComboAddress& ca, const std::string& callback_url,
                  const std::string& callback_pw, const std::string& encryption_key);

struct syncData {
  std::vector<std::pair<std::string, std::string>> sync_hosts;
  unsigned int min_sync_host_uptime;
  ComboAddress sibling_listen_addr;
  std::string  webserver_listen_addr;
  std::string  webserver_password;
};

void replicateOperation(const ReplicationOperation& rep_op);

extern syncData g_sync_data;
extern bool g_builtin_bl_enabled;
extern bool g_builtin_wl_enabled;
extern curlTLSOptions g_curl_tls_options;
