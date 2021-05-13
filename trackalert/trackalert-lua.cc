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
#include "trackalert.hh"
#include <thread>
#include "dolog.hh"
#include "sodcrypto.hh"
#include "base64.hh"
#include "trackalert-luastate.hh"
#include "perf-stats.hh"
#include "wforce-webserver.hh"
#include "trackalert-web.hh"
#include <fstream>
#include <boost/date_time/posix_time/posix_time.hpp>
#include "common-lua.hh"
#include "prometheus.hh"

using std::thread;

static vector<std::function<void(void)>>* g_launchWork;

// lua functions are split into three groups:
// 1) Those which are only applicable as "config/setup" (single global lua state) (they are defined as blank/empty functions otherwise) 
// 2) Those which are only applicable inside the "report" and "background" functions (multiple lua states running in different threads), which are defined as blank/empty otherwise
// 3) Those which are applicable to both states
// Functions that are in 2) or 3) MUST be thread-safe. The rest not as they are called at startup.
// We have a single lua config file for historical reasons, hence the somewhat complex structure of this function
// The Lua state and type is passed via "multi_lua" (true means it's one of the multiple states, false means it's the global lua config state) 
vector<std::function<void(void)>> setupLua(bool client, bool multi_lua,
                                           LuaContext& c_lua,
                                           report_t& report_func,
                                           bg_func_map_t* bg_func_map,
                                           CustomFuncMap& custom_func_map,
                                           const std::string& config)
{
  g_launchWork= new vector<std::function<void(void)>>();

  setupCommonLua(client, multi_lua, c_lua, config);

  c_lua.writeFunction("shutdown", []() { _exit(0);} );

  if (!multi_lua) {
    c_lua.writeFunction("webserver", [client](const std::string& address, const std::string& password) {
      if(client)
        return;
      ComboAddress local;
      try {
        local = ComboAddress(address);
      }
      catch (const WforceException& e) {
        errlog("webserver() error parsing address/port [%s]. Make sure to use IP addresses not hostnames", address);
        return;
      }
      try {
        int sock = socket(local.sin4.sin_family, SOCK_STREAM, 0);
        if (sock < 0) {
          throw std::runtime_error("Failed to create webserver socket");
        }
        SSetsockopt(sock, SOL_SOCKET, SO_REUSEADDR, 1);
        SBind(sock, local);
        SListen(sock, 1024);
        auto launch=[sock, local, password]() {
          thread t(WforceWebserver::start, sock, local, password, &g_webserver);
          t.detach();
        };
        if(g_launchWork)
          g_launchWork->push_back(launch);
        else
          launch();
      }
      catch(std::exception& e) {
        errlog("Unable to bind to webserver socket on %s: %s", local.toStringWithPort(), e.what());
        _exit(EXIT_FAILURE);
      }

    });
  }
  else {
    c_lua.writeFunction("webserver", [](const std::string& address, const std::string& password) { });
  }

  if (!multi_lua) {
    c_lua.writeFunction("controlSocket", [client](const std::string& str) {
      ComboAddress local;
      try {
        local = ComboAddress(str, 4005);
      }
      catch (const WforceException& e) {
        errlog("controlSocket() error parsing address/port [%s]. Make sure to use IP addresses not hostnames", str);
        return;
      }
      if(client) {
        g_serverControl = local;
        return;
      }

      try {
        int sock = socket(local.sin4.sin_family, SOCK_STREAM, 0);
        if (sock < 0) {
          throw std::runtime_error("Failed to create control socket");
        }
        SSetsockopt(sock, SOL_SOCKET, SO_REUSEADDR, 1);
        SBind(sock, local);
        SListen(sock, 5);
        auto launch=[sock, local]() {
          thread t(controlThread, sock, local);
          t.detach();
        };
        if(g_launchWork)
          g_launchWork->push_back(launch);
        else
          launch();

      }
      catch(std::exception& e) {
        errlog("Unable to bind to control socket on %s: %s", local.toStringWithPort(), e.what());
        _exit(EXIT_FAILURE);
      }
    });
  }
  else {
    c_lua.writeFunction("controlSocket", [](const std::string& str) { });
  }

  if (!multi_lua) {
    c_lua.writeFunction("stats", []() {
      boost::format fmt("%d reports\n");
      g_outputBuffer = (fmt % g_stats.reports).str();
    });
  }
  else {
    c_lua.writeFunction("stats", []() { });
  }

  if (!multi_lua) {
    c_lua.writeFunction("setNumReportThreads", [](int numThreads) {
      // XXX this function is deprecated - use setNumWorkerThreads instead
      infolog("setNumReportThreads is deprecated - use setNumWorkerThreads() instead");
    });
  }
  else {
    c_lua.writeFunction("setNumReportThreads", [](int numThreads) { });
  }


  if (multi_lua) {
    c_lua.writeFunction("setReport", [&report_func](report_t func) { report_func=func;});
  }
  else {
    c_lua.writeFunction("setReport", [](report_t func) { });
  }

  if (multi_lua) {
    c_lua.writeFunction("setBackground", [bg_func_map](const std::string& func_name, background_t func) {
      bg_func_map->insert(std::make_pair(func_name, func));
    });
  }
  else {
    c_lua.writeFunction("setBackground",  [](const std::string& func_name, background_t func) { });
  }

  if (!multi_lua) {
    c_lua.writeFunction("setNumSchedulerThreads", [](int numThreads) {
      g_bg_schedulerp->setNumThreads(numThreads);
    });
  }
  else {
    c_lua.writeFunction("setNumSchedulerThreads", [](int numThreads) { });
  }

  if (!multi_lua && !client) {
    c_lua.writeFunction("cronScheduleBackgroundFunc", [](const std::string& cron_str, const std::string& func_name) {
      g_bg_schedulerp->cron(cron_str, [func_name] {
        try {
          g_luamultip->background(func_name);
        }
        catch(LuaContext::ExecutionErrorException& e) {
          try {
            std::rethrow_if_nested(e);
            errlog("Lua background function [%s] exception: %s", func_name, e.what());
          }
          catch (const std::exception& ne) {
            errlog("Exception in background function [%s] exception: %s", func_name, ne.what());
          }
          catch (const WforceException& ne) {
            errlog("Exception in background function [%s] exception: %s", func_name, ne.reason);
          }
        }
      });
    });
  }
  else {
    c_lua.writeFunction("cronScheduleBackgroundFunc", [](const std::string& cron_str, const std::string& func_name) { });
  }

  if (!multi_lua && !client) {
    c_lua.writeFunction("intervalScheduleBackgroundFunc", [](const std::string& duration_str, const std::string& func_name) {
      std::stringstream ss(duration_str);
      boost::posix_time::time_duration td;
      if (ss >> td) {
        g_bg_schedulerp->interval(std::chrono::seconds(td.total_seconds()), [func_name] {
          try {
            g_luamultip->background(func_name);
          }
          catch(LuaContext::ExecutionErrorException& e) {
            try {
              std::rethrow_if_nested(e);
              errlog("Lua background function [%s] exception: %s", func_name, e.what());
            }
            catch (const std::exception& ne) {
              errlog("Exception in background function [%s] exception: %s", func_name, ne.what());
            }
            catch (const WforceException& ne) {
              errlog("Exception in background function [%s] exception: %s", func_name, ne.reason);
            }
          }
        });
      }
      else {
        std::string errmsg = "Cannot parse duration string: " + duration_str;
        errlog(errmsg.c_str());
        throw WforceException(errmsg);
      }
    });
  }
  else {
    c_lua.writeFunction("intervalScheduleBackgroundFunc", [](const std::string& cron_str, const std::string& func_name) { });
  }

  c_lua.writeFunction("durationToSeconds", [](const std::string& duration_str) {
    std::stringstream ss(duration_str);
    boost::posix_time::time_duration td;
    if (ss >> td) {
      return (long)td.total_seconds();
    }
    else {
      errlog("Cannot parse duration string: %s",duration_str);
      return (long)0;
    }
  });

  c_lua.registerMember("attrs", &CustomFuncArgs::attrs);
  c_lua.registerMember("attrs_mv", &CustomFuncArgs::attrs_mv);

  c_lua.writeFunction("setCustomEndpoint", [&custom_func_map, multi_lua, client](const std::string& f_name, custom_func_t func) {
    CustomFuncMapObject cobj;
    cobj.c_func = func;
    custom_func_map.insert(std::make_pair(f_name, cobj));
    if (!multi_lua && !client) {
      addCommandStat(f_name);
      addPrometheusCommandMetric(f_name);
      // register a webserver command
      g_webserver.registerFunc(f_name, HTTPVerb::POST, parseCustomCmd);
      noticelog("Registering custom endpoint [%s]", f_name);
    }
  });

  if (!multi_lua) {
    c_lua.writeFunction("showCustomEndpoints", []() {
      boost::format fmt("%-30.30s \n");
      g_outputBuffer = (fmt % "Custom Endpoint").str();
      for (const auto& i : g_custom_func_map) {
        g_outputBuffer += (fmt % i.first).str();
      }
    });
  }
  else {
    c_lua.writeFunction("showCustomEndpoints", []() { });
  }

  if (!multi_lua) {
    c_lua.writeFunction("showVersion", []() {
      g_outputBuffer = "trackalert " + std::string(VERSION) + "\n";
    });
  }
  else {
    c_lua.writeFunction("showVersion", []() { });
  }

  if (!multi_lua) {
    c_lua.writeFunction("setKey", [](const std::string& key) {
      string newkey;
      if(B64Decode(key, newkey) < 0) {
        g_outputBuffer=string("Unable to decode ")+key+" as Base64";
        errlog("%s", g_outputBuffer);
      }
      else
        g_key = newkey;
    });
  }
  else {
    c_lua.writeFunction("setKey", [](const std::string& key) { });
  }

  if (!multi_lua) {
    c_lua.writeFunction("testCrypto", [](string testmsg)
    {
      try {
        SodiumNonce sn, sn2;
        sn.init();
        sn2=sn;
        string encrypted = sodEncryptSym(testmsg, g_key, sn);
        string decrypted = sodDecryptSym(encrypted, g_key, sn2);

        sn.increment();
        sn2.increment();

        encrypted = sodEncryptSym(testmsg, g_key, sn);
        decrypted = sodDecryptSym(encrypted, g_key, sn2);

        if(testmsg == decrypted)
          g_outputBuffer="Everything is ok!\n";
        else
          g_outputBuffer="Crypto failed..\n";

      }
      catch(...) {
        g_outputBuffer="Crypto failed..\n";
      }});
  }
  else {
    c_lua.writeFunction("testCrypto", [](string testmsg) {});
  }

  std::ifstream ifs(config);
  if(!ifs)
    warnlog("Unable to read configuration from '%s'", config);
  else if (!multi_lua)
    infolog("Read configuration from '%s'", config);

  c_lua.executeCode(ifs);
  if (!multi_lua) {
    auto ret=*g_launchWork;
    delete g_launchWork;
    g_launchWork=0;
    return ret;
  }
  else {
    return vector<std::function<void(void)>>();
  }
}
