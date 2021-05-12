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
#include "perf-stats.hh"
#include "sodcrypto.hh"
#include "base64.hh"

extern std::mutex g_luamutex;
extern LuaContext g_lua;
extern std::string g_outputBuffer; // locking for this is ok, as locked by g_luamutex (functions using g_outputBuffer MUST NOT be enabled for the allow/report lua contexts)
extern WforceWebserver g_webserver;

extern GlobalStateHolder<NetmaskGroup> g_ACL;

extern WebHookRunner g_webhook_runner;
extern WebHookDB g_webhook_db;
extern WebHookDB g_custom_webhook_db;

extern int g_num_luastates;

void setupCommonLua(bool client,
                    bool multi_lua,
                    LuaContext& c_lua,
                    const std::string& config);
