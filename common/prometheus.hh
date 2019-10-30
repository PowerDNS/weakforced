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

#include "prometheus/registry.h"
#include "prometheus/text_serializer.h"
#include "prometheus/counter.h"
#include "prometheus/gauge.h"
#include "prometheus/histogram.h"
#include "wforce_ns.hh"
#include <string>

using namespace prometheus;

class PrometheusMetrics {
public:
  PrometheusMetrics(const std::string& prefix) : d_bb{0.001,0.01,0.1,1}, d_prefix(prefix)
  {
    d_registry = wforce::make_unique<Registry>();
    if (d_registry != nullptr){
      
      auto& wqd_family = BuildHistogram()
        .Name(d_prefix+"_worker_queue_duration_seconds")
        .Help("How many seconds do worker threads have to wait to run?")
        .Register(*d_registry);
      worker_queue_duration = &(wqd_family.Add({}, d_bb));
      auto& wrd_family = BuildHistogram()
        .Name(d_prefix+"_worker_response_duration_seconds")
        .Help("How many seconds do worker threads take to run?")
        .Register(*d_registry);
      worker_response_duration = &(wrd_family.Add({}, d_bb));
      auto& active_conn_family = BuildGauge()
        .Name(d_prefix+"_active_http_connections")
        .Help("How many active connections to the REST API?")
        .Register(*d_registry);
      active_connections = &(active_conn_family.Add({}));
      auto& web_queue_family = BuildGauge()
        .Name(d_prefix+"_web_queue_size")
        .Help("How full is the REST API worker thread queue?")
        .Register(*d_registry);
      web_queue_size = &(web_queue_family.Add({}));
      auto& webhook_queue_family = BuildGauge()
        .Name(d_prefix+"_webhook_queue_size")
        .Help("How full is the webhook worker thread queue?")
        .Register(*d_registry);
      webhook_queue_size = &(webhook_queue_family.Add({}));
      commands_family = &(BuildCounter()
                          .Name(d_prefix+"_commands_total")
                          .Help("How many commands have been received?")
                          .Register(*d_registry));
      custom_family = &(BuildCounter()
                          .Name(d_prefix+"_custom_stats_total")
                          .Help("Count of custom stats (by type)")
                          .Register(*d_registry));
      dns_query_family = &(BuildCounter()
                           .Name(d_prefix+"_dns_queries_total")
                           .Help("How many DNS queries were performed?")
                           .Register(*d_registry));
      dns_query_latency_family = &(BuildHistogram()
                                   .Name(d_prefix+"_dns_query_response_seconds")
                                   .Help("How long do DNS queries take?")
                                   .Register(*d_registry));
      webhook_family =  &(BuildCounter()
                           .Name(d_prefix+"_webhook_events_total")
                           .Help("How many webhook events occurred?")
                           .Register(*d_registry));
      custom_webhook_family =  &(BuildCounter()
                                 .Name(d_prefix+"_custom_webhook_events_total")
                                 .Help("How many custom webhook events occurred?")
                                 .Register(*d_registry));
    }
  }
  virtual ~PrometheusMetrics() = default;
  
  void addCommandMetric(const std::string& name);
  void incCommandMetric(const std::string& name);

  void addCustomMetric(const std::string& name);
  void incCustomMetric(const std::string& name);

  void observeWQD(float duration)
  {
    worker_queue_duration->Observe(duration);
  }

  void observeWRD(float duration)
  {
    worker_response_duration->Observe(duration);
  }
  
  void setActiveConns(int value)
  {
    active_connections->Set(value);
  }

  void addDNSResolverMetric(const std::string& name);
  void incDNSResolverMetric(const std::string& name);
  void observeDNSResolverLatency(const std::string& name, float duration);

  void addWebhookMetric(unsigned int id, const std::string& url, bool custom);
  void incWebhookMetric(unsigned int id, bool success, bool custom);

  void setWebQueueSize(int value)
  {
    web_queue_size->Set(value);
  }
  
  void setWebhookQueueSize(int value)
  {
    webhook_queue_size->Set(value);
  }

  std::string serialize()
  {
    return d_textSerializer.Serialize(d_registry->Collect());
  }

private:
  Histogram* worker_queue_duration;
  Histogram* worker_response_duration;
  Gauge* active_connections;
  Gauge* web_queue_size;
  Gauge* webhook_queue_size;
  Family<Counter>* commands_family;
  std::map<std::string, Counter*> commands_metrics;
  Family<Counter>* custom_family;
  std::map<std::string, Counter*> custom_metrics;
  Family<Counter>* dns_query_family;
  std::map<std::string, Counter*> dns_query_metrics;
  Family<Histogram>* dns_query_latency_family;
  std::map<std::string, Histogram*> dns_query_latency_metrics;
  Family<Counter>* webhook_family;
  std::map<std::string, Counter*> webhook_metrics;
  Family<Counter>* custom_webhook_family;
  std::map<std::string, Counter*> custom_webhook_metrics;
protected:
  prometheus::TextSerializer d_textSerializer;
  std::unique_ptr<prometheus::Registry> d_registry;
  Histogram::BucketBoundaries d_bb;
  std::string d_prefix;
};

void initPrometheusMetrics(std::shared_ptr<PrometheusMetrics> pmp);

void setPrometheusActiveConns(int value);

void addPrometheusCommandMetric(const std::string& name);
void incPrometheusCommandMetric(const std::string& name);
void addPrometheusCustomMetric(const std::string& name);
void incPrometheusCustomMetric(const std::string& name);

void observePrometheusWQD(float duration);
void observePrometheusWRD(float duration);

void addPrometheusDNSResolverMetric(const std::string& name);
void incPrometheusDNSResolverMetric(const std::string& name);
void observePrometheusDNSResolverLatency(const std::string& name, float duration);

void addPrometheusWebhookMetric(unsigned int id, const std::string& url, bool custom);
void incPrometheusWebhookMetric(unsigned int id, bool success, bool custom);

void setPrometheusWebQueueSize(int value);
void setPrometheusWebhookQueueSize(int value);

std::string serializePrometheusMetrics();
