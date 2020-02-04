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

#include "prometheus.hh"

class WforcePrometheus : public PrometheusMetrics
{
public:
  WforcePrometheus(const std::string& prefix) : PrometheusMetrics(prefix)
  {
    if (d_registry != nullptr) {
      allow_status_family = &(BuildCounter()
                              .Name(d_prefix+"_allow_status_total")
                              .Help("Count of different allow status responses")
                              .Register(*d_registry));
      repl_sent_family = &(BuildCounter()
                           .Name(d_prefix+"_replication_sent_total")
                           .Help("How many replication messages were sent?")
                           .Register(*d_registry));
      repl_rcvd_family = &(BuildCounter()
                           .Name(d_prefix+"_replication_rcvd_total")
                           .Help("How many replication messages were rcvd?")
                           .Register(*d_registry));
      repl_connfail_family = &(BuildCounter()
                           .Name(d_prefix+"_replication_tcp_connfailed_total")
                           .Help("How many TCP connections failed?")
                           .Register(*d_registry));
      auto& redis_wlbl_family = BuildCounter()
        .Name(d_prefix+"_redis_wlbl_updates_total")
        .Help("How many redis wl/bl updates succeeded?")
        .Register(*d_registry);
      redis_bl_updates = &(redis_wlbl_family.Add({{"type", "bl"}}));
      redis_wl_updates = &(redis_wlbl_family.Add({{"type", "wl"}}));
      auto& redis_wlbl_connfail_family = BuildCounter()
        .Name(d_prefix+"_redis_wlbl_connfailed_total")
        .Help("How many redis wl/bl connection attempts failed?")
        .Register(*d_registry);
      redis_bl_connfail = &(redis_wlbl_connfail_family.Add({{"type", "bl"}}));
      redis_wl_connfail = &(redis_wlbl_connfail_family.Add({{"type", "wl"}}));
      auto& bl_entries_family = BuildGauge()
        .Name(d_prefix+"_bl_entries")
        .Help("How many entries are in the blacklist?")
        .Register(*d_registry);
      bl_entries_ip = &(bl_entries_family.Add({{"type", "ip"}}));
      bl_entries_login = &(bl_entries_family.Add({{"type", "login"}}));
      bl_entries_iplogin = &(bl_entries_family.Add({{"type", "iplogin"}}));
      auto& wl_entries_family = BuildGauge()
        .Name(d_prefix+"_wl_entries")
        .Help("How many entries are in the whitelist?")
        .Register(*d_registry);
      wl_entries_ip = &(wl_entries_family.Add({{"type", "ip"}}));
      wl_entries_login = &(wl_entries_family.Add({{"type", "login"}}));
      wl_entries_iplogin = &(wl_entries_family.Add({{"type", "iplogin"}}));
      auto& repl_recv_queue_family = BuildGauge()
        .Name(d_prefix+"_repl_recv_queue_size")
        .Help("How full is the replication recv worker thread queue?")
        .Register(*d_registry);
      repl_recv_queue_size = &(repl_recv_queue_family.Add({}));
    }
  }

  void addAllowStatusMetric(const std::string& name);
  void incAllowStatusMetric(const std::string& name);

  void addReplicationSibling(const std::string& name);
  void incReplicationSent(const std::string& name, bool success);
  void incReplicationRcvd(const std::string& name, bool success);
  void incReplicationConnFail(const std::string& name);

  void incRedisBLUpdates();
  void incRedisBLConnFailed();
  void incRedisWLUpdates();
  void incRedisWLConnFailed();
  void setBLIPEntries(int);
  void setWLIPEntries(int);
  void setBLLoginEntries(int);
  void setWLLoginEntries(int);
  void setBLIPLoginEntries(int);
  void setWLIPLoginEntries(int);

  void setReplRecvQueueSize(int value)
  {
    repl_recv_queue_size->Set(value);
  }
  
private:
  Family<Counter>* allow_status_family;
  std::map<std::string, Counter*> allow_status_metrics;
  Family<Counter>* repl_sent_family;
  std::map<std::string, Counter*> repl_sent_ok_metrics;
  std::map<std::string, Counter*> repl_sent_err_metrics;
  Family<Counter>* repl_rcvd_family;
  std::map<std::string, Counter*> repl_rcvd_ok_metrics;
  std::map<std::string, Counter*> repl_rcvd_err_metrics;
  Family<Counter>* repl_connfail_family;
  std::map<std::string, Counter*> repl_connfail_metrics;
  Counter* redis_bl_updates;
  Counter* redis_bl_connfail;
  Counter* redis_wl_updates;
  Counter* redis_wl_connfail;
  Gauge* bl_entries_ip;
  Gauge* bl_entries_login;
  Gauge* bl_entries_iplogin;
  Gauge* wl_entries_ip;
  Gauge* wl_entries_login;
  Gauge* wl_entries_iplogin;
  Gauge* repl_recv_queue_size;
};

void initWforcePrometheusMetrics(std::shared_ptr<WforcePrometheus> wpmp);
void addPrometheusAllowStatusMetric(const std::string& name);
void incPrometheusAllowStatusMetric(const std::string& name);

void addPrometheusReplicationSibling(const std::string& name);
void incPrometheusReplicationSent(const std::string& name, bool success);
void incPrometheusReplicationRcvd(const std::string& name, bool success);
void incPrometheusReplicationConnFail(const std::string& name);

void incPrometheusRedisBLUpdates();
void incPrometheusRedisBLConnFailed();
void incPrometheusRedisWLUpdates();
void incPrometheusRedisWLConnFailed();
void setPrometheusBLIPEntries(int);
void setPrometheusWLIPEntries(int);
void setPrometheusBLLoginEntries(int);
void setPrometheusWLLoginEntries(int);
void setPrometheusBLIPLoginEntries(int);
void setPrometheusWLIPLoginEntries(int);

void setPrometheusReplRecvQueueSize(int value);
