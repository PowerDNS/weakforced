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

#include <unistd.h>
#include "json11.hpp"

namespace PerfStats {
  enum PerfStat { WorkerThreadWait_0_1=0, WorkerThreadWait_1_10=2, WorkerThreadWait_10_100=3, WorkerThreadWait_100_1000=4, WorkerThreadWait_Slow=5, WorkerThreadRun_0_1=6, WorkerThreadRun_1_10=7, WorkerThreadRun_10_100=8, WorkerThreadRun_100_1000=9, WorkerThreadRun_Slow=10 }; 

  const std::string wtw_0_1_str = "WTW_0_1";
  const std::string wtw_1_10_str = "WTW_1_10";
  const std::string wtw_10_100_str = "WTW_10_100";
  const std::string wtw_100_1000_str = "WTW_100_1000";
  const std::string wtw_slow_str = "WTW_Slow";
  const std::string wtr_0_1_str = "WTR_0_1";
  const std::string wtr_1_10_str = "WTR_1_10";
  const std::string wtr_10_100_str = "WTR_10_100";
  const std::string wtr_100_1000_str = "WTR_100_1000";
  const std::string wtr_slow_str = "WTR_Slow";
  const std::string command_str = "Command";
  const std::string custom_str = "Custom";
};

void initPerfStats();
void addWTWStat(unsigned int num_ms); // number of milliseconds
void addWTRStat(unsigned int num_ms); // number of milliseconds
void startStatsThread();
std::string getPerfStatsString(); // return perf stats in a string
json11::Json perfStatsToJson(); // return perf stats as a json object

void addCommandStat(const std::string& command_name);
void incCommandStat(const std::string& command_name);
int getCommandStat(const std::string& command_name);
void addCustomStat(const std::string& custom_name);
void incCustomStat(const std::string& custom_name);
int getCustomStat(const std::string& custom_name);
json11::Json commandStatsToJson();
json11::Json customStatsToJson();
std::string getCommandStatsString();
std::string getCustomStatsString();
