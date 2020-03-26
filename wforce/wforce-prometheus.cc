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

#include "wforce-prometheus.hh"

static std::shared_ptr<WforcePrometheus> wforce_prom_metrics;

void initWforcePrometheusMetrics(std::shared_ptr<WforcePrometheus> wpmp)
{
  wforce_prom_metrics = wpmp;
  initPrometheusMetrics(wpmp);
}

void WforcePrometheus::addAllowStatusMetric(const std::string& name)
{
  Counter* c = &(allow_status_family->Add({{"status", name}}));
  allow_status_metrics.insert(std::make_pair(name, c));
}


void WforcePrometheus::incAllowStatusMetric(const std::string& name)
{
  auto i = allow_status_metrics.find(name);
  if (i != allow_status_metrics.end()) {
    i->second->Increment();
  }
}

void WforcePrometheus::addReplicationSibling(const std::string& name)
{
  Counter* c = &(repl_sent_family->Add({{"sibling", name}, {"status", "ok"}}));
  repl_sent_ok_metrics.insert(std::make_pair(name, c));
  c = &(repl_sent_family->Add({{"sibling", name}, {"status", "error"}}));
  repl_sent_err_metrics.insert(std::make_pair(name, c));
  c = &(repl_rcvd_family->Add({{"sibling", name}, {"status", "ok"}}));
  repl_rcvd_ok_metrics.insert(std::make_pair(name, c));
  c = &(repl_rcvd_family->Add({{"sibling", name}, {"status", "error"}}));
  repl_rcvd_err_metrics.insert(std::make_pair(name, c));
  c = &(repl_connfail_family->Add({{"sibling", name}}));
  repl_connfail_metrics.insert(std::make_pair(name, c));
}

void WforcePrometheus::incReplicationSent(const std::string& name, bool success)
{
  if (success == true) {
    auto i = repl_sent_ok_metrics.find(name);
    if (i != repl_sent_ok_metrics.end()) {
      i->second->Increment();
    }
  }
  else {
    auto i = repl_sent_err_metrics.find(name);
    if (i != repl_sent_err_metrics.end()) {
      i->second->Increment();
    }
  }
}
void WforcePrometheus::incReplicationRcvd(const std::string& name, bool success)
{
  if (success == true) {
    auto i = repl_rcvd_ok_metrics.find(name);
    if (i != repl_rcvd_ok_metrics.end()) {
      i->second->Increment();
    }
  }
  else {
    auto i = repl_rcvd_err_metrics.find(name);
    if (i != repl_rcvd_err_metrics.end()) {
      i->second->Increment();
    }
  }
}

void WforcePrometheus::incReplicationConnFail(const std::string& name)
{
  auto i = repl_connfail_metrics.find(name);
  if (i != repl_connfail_metrics.end()) {
    i->second->Increment();
  }  
}

void WforcePrometheus::incRedisBLUpdates()
{
  if (redis_bl_updates != nullptr)
    redis_bl_updates->Increment();
}

void WforcePrometheus::incRedisWLUpdates()
{
  if (redis_wl_updates != nullptr)
    redis_wl_updates->Increment();
}

void WforcePrometheus::incRedisBLConnFailed()
{
  if (redis_bl_connfail != nullptr)
    redis_bl_connfail->Increment();
}

void WforcePrometheus::incRedisWLConnFailed()
{
  if (redis_wl_connfail != nullptr)
    redis_wl_connfail->Increment();
}

void WforcePrometheus::setBLIPEntries(int num_entries)
{
  if (bl_entries_ip != nullptr)
    bl_entries_ip->Set(num_entries);
}

void WforcePrometheus::setBLLoginEntries(int num_entries)
{
  if (bl_entries_login != nullptr)
    bl_entries_login->Set(num_entries);
}

void WforcePrometheus::setBLIPLoginEntries(int num_entries)
{
  if (bl_entries_iplogin != nullptr)
    bl_entries_iplogin->Set(num_entries);
}

void WforcePrometheus::setWLIPEntries(int num_entries)
{
  if (wl_entries_ip != nullptr)
    wl_entries_ip->Set(num_entries);
}

void WforcePrometheus::setWLLoginEntries(int num_entries)
{
  if (wl_entries_login != nullptr)
    wl_entries_login->Set(num_entries);
}

void WforcePrometheus::setWLIPLoginEntries(int num_entries)
{
  if (wl_entries_iplogin != nullptr)
    wl_entries_iplogin->Set(num_entries);
}

void addPrometheusAllowStatusMetric(const std::string& name)
{
  if (wforce_prom_metrics != nullptr) {
    wforce_prom_metrics->addAllowStatusMetric(name);
  }
}

void incPrometheusAllowStatusMetric(const std::string& name)
{
  if (wforce_prom_metrics != nullptr) {
    wforce_prom_metrics->incAllowStatusMetric(name);
  }
}

void addPrometheusReplicationSibling(const std::string& name)
{
  if (wforce_prom_metrics != nullptr) {
    wforce_prom_metrics->addReplicationSibling(name);
  }
}

void incPrometheusReplicationSent(const std::string& name, bool success) {
  if (wforce_prom_metrics != nullptr) {
    wforce_prom_metrics->incReplicationSent(name, success);
  }
}

void incPrometheusReplicationRcvd(const std::string& name, bool success) {
  if (wforce_prom_metrics != nullptr) {
    wforce_prom_metrics->incReplicationRcvd(name, success);
  }
}
void incPrometheusReplicationConnFail(const std::string& name) {
  if (wforce_prom_metrics != nullptr) {
    wforce_prom_metrics->incReplicationConnFail(name);
  }
}

void incPrometheusRedisBLUpdates()
{
  if (wforce_prom_metrics != nullptr) {
    wforce_prom_metrics->incRedisBLUpdates();
  }
}

void incPrometheusRedisBLConnFailed()
{
  if (wforce_prom_metrics != nullptr) {
    wforce_prom_metrics->incRedisBLConnFailed();
  }
}
void incPrometheusRedisWLUpdates()
{
  if (wforce_prom_metrics != nullptr) {
    wforce_prom_metrics->incRedisWLUpdates();
  }
}

void incPrometheusRedisWLConnFailed()
{
if (wforce_prom_metrics != nullptr) {
    wforce_prom_metrics->incRedisWLConnFailed();
  }
}

void setPrometheusBLIPEntries(int num_entries)
{
  if (wforce_prom_metrics != nullptr) {
    wforce_prom_metrics->setBLIPEntries(num_entries);
  }
}
void setPrometheusWLIPEntries(int num_entries)
{
  if (wforce_prom_metrics != nullptr) {
    wforce_prom_metrics->setWLIPEntries(num_entries);
  }
}
void setPrometheusBLLoginEntries(int num_entries)
{
  if (wforce_prom_metrics != nullptr) {
    wforce_prom_metrics->setBLLoginEntries(num_entries);
  }
}

void setPrometheusWLLoginEntries(int num_entries)
{
  if (wforce_prom_metrics != nullptr) {
    wforce_prom_metrics->setWLLoginEntries(num_entries);
  }
}

void setPrometheusBLIPLoginEntries(int num_entries)
{
  if (wforce_prom_metrics != nullptr) {
    wforce_prom_metrics->setBLIPLoginEntries(num_entries);
  }
}

void setPrometheusWLIPLoginEntries(int num_entries)
{
  if (wforce_prom_metrics != nullptr) {
    wforce_prom_metrics->setWLIPLoginEntries(num_entries);
  }
}

void setPrometheusReplRecvQueueSize(int value)
{
  if (wforce_prom_metrics) {
    wforce_prom_metrics->setReplRecvQueueSize(value);
  }
}

void setPrometheusReplRecvQueueRetrieveFunc(int (*func)())
{
  if (wforce_prom_metrics) {
    wforce_prom_metrics->setReplRecvQueueRetrieveFunc(func);
  }
}
