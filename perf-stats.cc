/*
 * This file is part of PowerDNS or weakforced.
 * Copyright -- PowerDNS.COM B.V. and its contributors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
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

#define STATS_NUM_WINDOWS 5
#define STATS_WINDOW_SIZE 60

TWStatsDB<unsigned int> g_perfStats("perfStats", STATS_WINDOW_SIZE, STATS_NUM_WINDOWS);

FieldMap fm = { { "WTW_0_1", "int" },
		{ "WTW_1_10", "int" },
		{ "WTW_10_100", "int" },
		{ "WTW_100_1000", "int" },
		{ "WTW_Slow", "int" },
		{ "WTR_0_1", "int" },
		{ "WTR_1_10", "int" },
		{ "WTR_10_100", "int" },
		{ "WTR_100_1000", "int" },
		{ "WTR_Slow", "int" } };

std::map<PerfStat, std::string> lookupPerfStat = { { WorkerThreadWait_0_1, "WTW_0_1" },
						   { WorkerThreadWait_1_10, "WTW_1_10" }, 
						   { WorkerThreadWait_10_100, "WTW_10_100" },
						   { WorkerThreadWait_100_1000, "WTW_100_1000" },
						   { WorkerThreadWait_Slow, "WTW_Slow" }, 
						   { WorkerThreadRun_0_1, "WTR_0_1" },
						   { WorkerThreadRun_1_10, "WTR_1_10" },
						   { WorkerThreadRun_10_100, "WTR_10_100" },
						   { WorkerThreadRun_100_1000, "WTR_100_1000" },
						   { WorkerThreadRun_Slow, "WTR_Slow" } };

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

void statsReportingThread()
{
  int interval = STATS_WINDOW_SIZE*STATS_NUM_WINDOWS;

  for (;;) {
    std::stringstream ss;
    
    sleep(interval);

    ss << "stats last " << interval << " secs: ";
    for (auto i=lookupPerfStat.begin(); i!=lookupPerfStat.end(); ++i) {
      ss << i->second << "=" << getStat(i->first) << " ";
    }
    warnlog("%s", ss.str());
  }
}

void startStatsThread()
{
  warnlog("Starting stats reporting thread");
  initPerfStats();
  thread t(statsReportingThread);
  t.detach();
}
