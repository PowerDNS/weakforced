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

#include <iostream>
#include <unistd.h>
#include <thread>
#include <sstream>
#include "perf-stats.hh"
#include "twmap.hh"
#include "dolog.hh"

#define STATS_NUM_WINDOWS 20
#define STATS_WINDOW_SIZE 15

using namespace PerfStats;

TWStatsDB<unsigned int> g_perfStats("perfStats", STATS_WINDOW_SIZE, STATS_NUM_WINDOWS);

const static FieldMap fm = { { wtw_0_1_str, "int" },
                { wtw_1_10_str, "int" },
                { wtw_10_100_str, "int" },
                { wtw_100_1000_str, "int" },
                { wtw_slow_str, "int" },
                { wtr_0_1_str, "int" },
                { wtr_1_10_str, "int" },
                { wtr_10_100_str, "int" },
                { wtr_100_1000_str, "int" },
                { wtr_slow_str, "int" },
                { command_str, "countmin"},
                { custom_str, "countmin"} };

std::map<PerfStat, std::string> lookupPerfStat = { { WorkerThreadWait_0_1, wtw_0_1_str },
                                                   { WorkerThreadWait_1_10, wtw_1_10_str }, 
                                                   { WorkerThreadWait_10_100, wtw_10_100_str },
                                                   { WorkerThreadWait_100_1000,  wtw_100_1000_str},
                                                   { WorkerThreadWait_Slow, wtw_slow_str }, 
                                                   { WorkerThreadRun_0_1, wtr_0_1_str },
                                                   { WorkerThreadRun_1_10, wtr_1_10_str },
                                                   { WorkerThreadRun_10_100, wtr_10_100_str },
                                                   { WorkerThreadRun_100_1000, wtr_100_1000_str },
                                                   { WorkerThreadRun_Slow, wtr_slow_str } };

void initPerfStats()
{
  g_perfStats.setFields(fm);
}

void incStat(PerfStat stat)
{
  auto i = lookupPerfStat.find(stat);
  if (i != lookupPerfStat.end())
    g_perfStats.add(1, i->second, 1);
}

void addWTWStat(unsigned int num_ms) {
  if (num_ms <= 1)
    incStat(WorkerThreadWait_0_1);
  else if (num_ms <= 10)
    incStat(WorkerThreadWait_1_10);
  else if (num_ms <= 100)
    incStat(WorkerThreadWait_10_100);
  else if (num_ms <= 1000)
    incStat(WorkerThreadWait_100_1000);
  else
    incStat(WorkerThreadWait_Slow);
}

void addWTRStat(unsigned int num_ms) {
  if (num_ms <= 1)
    incStat(WorkerThreadRun_0_1);
  else if (num_ms <= 10)
    incStat(WorkerThreadRun_1_10);
  else if (num_ms <= 100)
    incStat(WorkerThreadRun_10_100);
  else if (num_ms <= 1000)
    incStat(WorkerThreadRun_100_1000);
  else
    incStat(WorkerThreadRun_Slow);
}

int getStat(PerfStat stat)
{
  auto i = lookupPerfStat.find(stat);
  if (i != lookupPerfStat.end())
    return g_perfStats.get(1, i->second);
  else
    return 0;
}

static std::set<std::string> command_stats;

void addCommandStat(const std::string& command_name)
{
  command_stats.insert(command_name);
}

void incCommandStat(const std::string& command_name)
{
  g_perfStats.add(1, "Command", command_name, 1);
}

int getCommandStat(const std::string& command_name)
{
  return g_perfStats.get(1, "Command", command_name);
}

static std::set<std::string> custom_stats;

void addCustomStat(const std::string& custom_name)
{
  custom_stats.insert(custom_name);
}

void incCustomStat(const std::string& custom_name)
{
  g_perfStats.add(1, "Custom", custom_name, 1);
}

int getCustomStat(const std::string& custom_name)
{
  return g_perfStats.get(1, "Custom", custom_name);
}

void statsReportingThread()
{
  setThreadName("wf/perf-stats");

  int interval = STATS_WINDOW_SIZE*STATS_NUM_WINDOWS;

  for (;;) {
    std::stringstream ss;
    
    sleep(interval);

    ss << "perf stats last " << interval << " secs: ";
    for (auto i=lookupPerfStat.begin(); i!=lookupPerfStat.end(); ++i) {
      ss << i->second << "=" << getStat(i->first) << " ";
    }
    noticelog("%s", ss.str());

    if (command_stats.size() != 0) {
      ss.str(std::string());
      ss.clear();
      ss << "command stats last " << interval << " secs: ";
      for (const auto& i : command_stats) {
        ss << i << "=" << getCommandStat(i) << " ";
      }
      noticelog("%s", ss.str());
    }

    if (custom_stats.size() != 0) {
      ss.str(std::string());
      ss.clear();
      ss << "custom stats last " << interval << " secs: ";
      for (const auto& i : custom_stats) {
        ss << i << "=" << getCustomStat(i) << " ";
      }
      noticelog("%s", ss.str());
    }
  }
}

void startStatsThread()
{
  infolog("Starting stats reporting thread");
  initPerfStats();
  thread t(statsReportingThread);
  t.detach();
}

std::string getPerfStatsString()
{
  std::stringstream ss;
  for (auto i=lookupPerfStat.begin(); i!=lookupPerfStat.end(); ++i) {
    ss << i->second << "=" << getStat(i->first) << "\n";
  }
  return ss.str();
}

std::string getCommandStatsString()
{
  std::ostringstream ss;
  for (const auto& i : command_stats) {
    ss << i << "=" << getCommandStat(i) << endl;
  }
  return ss.str();
}

std::string getCustomStatsString()
{
  std::ostringstream ss;
  for (const auto& i : custom_stats) {
    ss << i << "=" << getCustomStat(i) << endl;
  }
  return ss.str();
}

using namespace json11;
Json perfStatsToJson()
{
  Json::object jattrs;
  for (auto i=lookupPerfStat.begin(); i!=lookupPerfStat.end(); ++i) {
    jattrs.insert(std::make_pair(i->second, getStat(i->first)));
  }
  return jattrs;
}

Json commandStatsToJson()
{
  Json::object jattrs;
  for (const auto& i : command_stats) {
    jattrs.insert(std::make_pair(i, getCommandStat(i)));
  }
  return jattrs;
}

Json customStatsToJson()
{
  Json::object jattrs;
  for (const auto& i : custom_stats) {
    jattrs.insert(std::make_pair(i, getCustomStat(i)));
  }
  return jattrs;
}
