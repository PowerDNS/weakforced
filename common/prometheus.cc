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

#include "prometheus.hh"

static std::shared_ptr<PrometheusMetrics> prom_metrics;

void initPrometheusMetrics(std::shared_ptr<PrometheusMetrics> pmp)
{
  prom_metrics = pmp;
}

void PrometheusMetrics::addCommandMetric(const std::string& name)
{
  Counter* c = &(commands_family->Add({{"cmd", name}}));
  commands_metrics.insert(std::make_pair(name, c));
}

void PrometheusMetrics::incCommandMetric(const std::string& name)
{
  auto i = commands_metrics.find(name);
  if (i != commands_metrics.end()) {
    i->second->Increment();
  }
}

void PrometheusMetrics::addCustomMetric(const std::string& name)
{
  Counter* c = &(custom_family->Add({{"metric", name}}));
  custom_metrics.insert(std::make_pair(name, c));
}

void PrometheusMetrics::incCustomMetric(const std::string& name)
{
  auto i = custom_metrics.find(name);
  if (i != custom_metrics.end()) {
    i->second->Increment();
  }
}

void PrometheusMetrics::addDNSResolverMetric(const std::string& name)
{
  Counter* c = &(dns_query_family->Add({{"resolver", name}}));
  dns_query_metrics.insert(std::make_pair(name, c));
  Histogram* h = &(dns_query_latency_family->Add({{"resolver", name}}, d_bb));
  dns_query_latency_metrics.insert(std::make_pair(name, h));
}

void PrometheusMetrics::incDNSResolverMetric(const std::string& name)
{
  auto i = dns_query_metrics.find(name);
  if (i != dns_query_metrics.end()) {
    i->second->Increment();
  }
}

void PrometheusMetrics::observeDNSResolverLatency(const std::string& name, float duration)
{
  auto i = dns_query_latency_metrics.find(name);
  if (i != dns_query_latency_metrics.end()) {
    i->second->Observe(duration);
  }
}

void PrometheusMetrics::addWebhookMetric(unsigned int id, const std::string& url, bool custom)
{
  std::string success_key = std::to_string(id) + "ok";
  std::string error_key = std::to_string(id) + "error";
  if (!custom) {
    Counter* c = &(webhook_family->Add({{"id", std::to_string(id)}, {"url", url}, {"status", "ok"}}));
    webhook_metrics.insert(std::make_pair(success_key, c));
    c = &(webhook_family->Add({{"id", std::to_string(id)}, {"url", url}, {"status", "error"}}));
    webhook_metrics.insert(std::make_pair(error_key, c));
  }
  else {
    Counter* c = &(custom_webhook_family->Add({{"id", std::to_string(id)}, {"url", url}, {"status", "ok"}}));
    custom_webhook_metrics.insert(std::make_pair(success_key, c));
    c = &(custom_webhook_family->Add({{"id", std::to_string(id)}, {"url", url}, {"status", "error"}}));
    custom_webhook_metrics.insert(std::make_pair(error_key, c));
  }
}

void PrometheusMetrics::incWebhookMetric(unsigned int id, bool success, bool custom)
{
  std::string key = std::to_string(id) + (success ? "ok" : "error");
  if (!custom) {
    auto i = webhook_metrics.find(key);
    if (i != webhook_metrics.end()) {
      i->second->Increment();
    }
  }
  else {
    auto i = custom_webhook_metrics.find(key);
    if (i != custom_webhook_metrics.end()) {
      i->second->Increment();
    }
  }
}

void setPrometheusActiveConns(int value)
{
  if (prom_metrics)
    prom_metrics->setActiveConns(value);
}

void addPrometheusCommandMetric(const std::string& name)
{
  if (prom_metrics)
    prom_metrics->addCommandMetric(name);
}

void incPrometheusCommandMetric(const std::string& name)
{
  if (prom_metrics)
    prom_metrics->incCommandMetric(name);
}

void addPrometheusCustomMetric(const std::string& name)
{
  if (prom_metrics)
    prom_metrics->addCustomMetric(name);
}

void incPrometheusCustomMetric(const std::string& name)
{
  if (prom_metrics)
    prom_metrics->incCustomMetric(name);
}

void observePrometheusWQD(float duration)
{
  if (prom_metrics)
    prom_metrics->observeWQD(duration);
}

void observePrometheusWRD(float duration)
{
  if (prom_metrics)
    prom_metrics->observeWRD(duration);
}

void addPrometheusDNSResolverMetric(const std::string& name)
{
  if (prom_metrics) {
    prom_metrics->addDNSResolverMetric(name);
  }
}

void incPrometheusDNSResolverMetric(const std::string& name)
{
  if (prom_metrics) {
    prom_metrics->incDNSResolverMetric(name);
  }
}

void observePrometheusDNSResolverLatency(const std::string& name, float duration)
{
  if (prom_metrics) {
    prom_metrics->observeDNSResolverLatency(name, duration);
  }
}

void addPrometheusWebhookMetric(unsigned int id, const std::string& url, bool custom)
{
  if (prom_metrics) {
    prom_metrics->addWebhookMetric(id, url, custom);
  }
}

void incPrometheusWebhookMetric(unsigned int id, bool success, bool custom)
{
  if (prom_metrics) {
    prom_metrics->incWebhookMetric(id, success, custom);
  }
}

void setPrometheusWebQueueSize(int value)
{
  if (prom_metrics) {
    prom_metrics->setWebQueueSize(value);
  }
}

void setPrometheusWebhookQueueSize(int value)
{
  if (prom_metrics) {
    prom_metrics->setWebhookQueueSize(value);
  }
}

std::string serializePrometheusMetrics()
{
  if (prom_metrics)
    return prom_metrics->serialize();
  else
    return std::string("Prometheus Metrics not initialized");
}
