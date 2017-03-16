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

#include <unistd.h>

enum PerfStat { WorkerThreadWait_0_1=0, WorkerThreadWait_1_10=2, WorkerThreadWait_10_100=3, WorkerThreadWait_100_1000=4, WorkerThreadWait_Slow=5, WorkerThreadRun_0_1=6, WorkerThreadRun_1_10=7, WorkerThreadRun_10_100=8, WorkerThreadRun_100_1000=9, WorkerThreadRun_Slow=10 }; 

void initPerfStats();
void addWTWStat(unsigned int num_ms); // number of milliseconds
void addWTRStat(unsigned int num_ms); // number of milliseconds
void startStatsThread();
std::string getPerfStatsString(); // return perf stats in a string
