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
#include <mutex>
#include <curl/curl.h>
#include "json11.hpp"
#include "ext/ctpl.h"
#include "dolog.hh"
#include "minicurl.hh"
#include "prometheus.hh"
#include <queue>

using namespace json11;

using WHConfigMap = std::map<std::string, std::string>;
using WHEvents = std::vector<std::string>;
// Key = event name, value is a pair of config key vectors -
// first is mandatory, second is optional
using WHEventConfig = std::pair<std::vector<std::string>, std::vector<std::string>>;
using WHEventTypes = std::map<std::string, WHEventConfig>;

class WebHook {
public:
  WebHook(unsigned int wh_id, const WHEvents& wh_events, bool wh_active, const WHConfigMap& wh_config_keys) :
    id(wh_id), active(wh_active), config_keys(wh_config_keys) {
    addEvents(wh_events);
  }
  WebHook(unsigned int wh_id, bool wh_active, const WHConfigMap& wh_config_keys, const std::string& wh_name) :
    id(wh_id), active(wh_active), config_keys(wh_config_keys), name(wh_name)  {
  }
  virtual ~WebHook() {}
  virtual bool operator==(const WebHook& rhs)
  {
    return id == rhs.getID();
  }
  unsigned int getID() const { return id; }
  const std::string getName() const { return name; }
  bool hasName() const {
    if (name.compare("") == 0)
      return false;
    else
      return true;
  }
  WHEvents getEventNames() const
  {
    WHEvents ret_events;
    for (auto& i : event_names) {
      ret_events.push_back(i.first);
    }
    return ret_events;
  }
  bool validEventName(const std::string& event_name) const
  {
    for (auto& i : event_names) {
      if (i.first.compare(event_name) == 0)
	return true;
    }
    return false;
  }
  const WHEvents& getEvents() const
  {
    std::lock_guard<std::mutex> lock(mutex);
    return events;
  }
  std::string getEventsStr() const 
  {
    std::lock_guard<std::mutex> lock(mutex);
    std::stringstream ss;
    for (auto& i : events) {
      ss << i << " ";
    }
    return ss.str();
  }
  const WHEvents& setEvents(const WHEvents& new_events)
  {
    std::lock_guard<std::mutex> lock(mutex);
    events = new_events;
    return events;
  }
  const WHEvents& addEvents(const WHEvents& new_events)
  {
    for (auto& i : new_events) {
      addEvent(i);
    }
    return events;
  }
  const WHEvents& addEvent(const std::string& event_name)
  {
    std::lock_guard<std::mutex> lock(mutex);
    if (validEventName(event_name)) {
      bool duplicate = false;
      for (auto& i : events) {
	if (i.compare(event_name) == 0) {
	  duplicate = true;
	  break;
	}
      }
      if (!duplicate)
	events.push_back(event_name);
    }
    return events;
  }
  const WHEvents& deleteEvents(const WHEvents& new_events)
  {
    for (auto& i : new_events) {
      deleteEvent(i);
    }
    return events;
  }
  const WHEvents& deleteEvent(const std::string& event_name)
  {
    std::lock_guard<std::mutex> lock(mutex);
    for (auto i=events.begin(); i!=events.end(); ) {
      if (i->compare(event_name) == 0) {
	i = events.erase(i);
	break;
      }
      else
	++i;
    }
    return events;
  }
  // returns true if config is OK
  virtual bool validateConfig(std::string& error_msg) const
  {
    std::lock_guard<std::mutex> lock(mutex);
    if (event_names.begin() == event_names.end()) {
      error_msg = "No events registered";
      return false;
    }
    // go through the configured events and ensure we have the right
    // mandatory config keys
    for (auto& ev : event_names ) {
      std::vector<std::string> man_cfg = ev.second.first;

      for (auto& cf : man_cfg) {
	if (config_keys.find(cf) == config_keys.end()) {
	  error_msg = "Missing mandatory configuration key: " + cf;
	  return false;
	}
      }
    }
    return true;
  }  
  const WHEventConfig& getConfigForEvent(std::string event_name) const
  {
    std::lock_guard<std::mutex> lock(mutex);
    static const WHEventConfig null_config;

    if (validEventName(event_name)) {
      for (auto& i : event_names) {
	if (i.first.compare(event_name) == 0)
	  return i.second;
      }
    }
    return null_config;
  }
  const WHConfigMap& getConfigKeys() const
  {
    std::lock_guard<std::mutex> lock(mutex);
    return config_keys;
  }
  bool hasConfigKey(const std::string& key) const {
    std::lock_guard<std::mutex> lock(mutex);
    auto i = config_keys.find(key);
    if (i != config_keys.end())
      return true;
    return false;
  }
  std::string getConfigKey(const std::string& key) const {
    std::lock_guard<std::mutex> lock(mutex);
    auto i = config_keys.find(key);
    if (i != config_keys.end())
      return i->second;
    else
      return "";
  }
  const WHConfigMap& setConfigKeys(const WHConfigMap& new_cfg)
  {
    std::lock_guard<std::mutex> lock(mutex);
    config_keys = new_cfg;
    return config_keys;
  }
  const WHConfigMap& addConfigKeys(const WHConfigMap& new_cfg)
  {
    std::lock_guard<std::mutex> lock(mutex);
    for (const auto& i : new_cfg) {
      config_keys.insert(std::make_pair(i.first, i.second));
    }
    return config_keys;
  }
  const WHConfigMap& addConfigKey(const std::string& key, const std::string& val)
  {
    std::lock_guard<std::mutex> lock(mutex);
    config_keys.insert(std::make_pair(key, val));
    return config_keys;
  }
  const WHConfigMap& deleteConfigKey(const std::string& key)
  {
    std::lock_guard<std::mutex> lock(mutex);
    config_keys.erase(key);
    return config_keys;
  }
  bool isActive() const
  {
    std::lock_guard<std::mutex> lock(mutex);
    return active;
  }
  void setActive(bool isActive)
  {
    std::lock_guard<std::mutex> lock(mutex);
    active = isActive;
  }
  void toString(std::string& out) const
  {
    std::lock_guard<std::mutex> lock(mutex);
    Json my_object = to_json();
    my_object.dump(out);
  }
  std::string toString() const
  {
    std::string out;
    toString(out);
    return out;
  }
  virtual Json to_json() const
  {
    std::lock_guard<std::mutex> lock(mutex);
    Json::array jevents;
    Json my_object = Json::object {
      { "id", (int)id },
      { "events", events },
      { "config", config_keys }
    };
    return my_object;
  }
  virtual void incSuccess() const
  {
    num_success++;
    incPrometheusWebhookMetric(id, true, false);
  }
  unsigned int getSuccess() const
  {
    return num_success;
  }
  virtual void incFailed() const
  {
    num_failed++;
    incPrometheusWebhookMetric(id, false, false);
  }
  unsigned int getFailed() const
  {
    return num_failed;
  }
protected:
  unsigned int id;
  std::vector<std::string> events;
  bool active;
  WHConfigMap config_keys;
  mutable std::atomic<unsigned int> num_success{0};
  mutable std::atomic<unsigned int> num_failed{0};
  mutable std::mutex mutex;
  const WHEventTypes event_names = { { "report", {{ "url" }, {"secret", "basic-auth"}}},
				     { "allow", {{ "url" }, {"secret", "allow_filter", "basic-auth"}}},
				     { "reset", {{ "url" }, {"secret", "basic-auth"}}},
				     { "addbl", {{ "url" }, {"secret", "basic-auth"}}},
				     { "delbl", {{ "url" }, {"secret", "basic-auth"}}},
				     { "expirebl", {{ "url" }, {"secret", "basic-auth"}}},
				     { "addwl", {{ "url" }, {"secret", "basic-auth"}}},
				     { "delwl", {{ "url" }, {"secret", "basic-auth"}}},
				     { "expirewl", {{ "url" }, {"secret", "basic-auth"}}}
  };
  const std::string name;
};

class CustomWebHook : public WebHook {
public:
  CustomWebHook(unsigned int wh_id, const std::string& wh_name, bool wh_active,
		const WHConfigMap& wh_config_keys) : WebHook(wh_id, wh_active, wh_config_keys, wh_name)  {
  }
  ~CustomWebHook() {}
  bool operator==(const WebHook& rhs) override
  {
    return ((id == rhs.getID()) && (name.compare(rhs.getName()) == 0));
  }
    // returns true if config is OK
  bool validateConfig(std::string& error_msg) const override
  {
    std::lock_guard<std::mutex> lock(mutex);
    if (config_keys.find("url") == config_keys.end()) {
	  error_msg = "Missing mandatory configuration key: url";
	  return false;
    }
    return true;
  }
  void incSuccess() const override
  {
    num_success++;
    incPrometheusWebhookMetric(id, true, true);
  }
  void incFailed() const override
  {
    num_failed++;
    incPrometheusWebhookMetric(id, false, true);
  }
  Json to_json() const override
  {
    std::lock_guard<std::mutex> lock(mutex);
    Json::array jevents;
    Json my_object = Json::object {
      { "id", (int)id },
      { "name", name },
      { "config", config_keys }
    };
    return my_object;
  }
};

class WebHookDB
{
public:
  WebHookDB():id(0)
  {
  }
  unsigned int getNewID() const { return ++id; }
  bool addWebHook(const WebHook& hook, std::string&error_msg)
  {
    std::lock_guard<std::mutex> lock(mutex);
    bool retval = true;
    
    if (hook.validateConfig(error_msg)) {
      for (const auto& i : webhooks) {
	if (i->getID() == hook.getID()) {
	  error_msg = "Registering webhook failed: id=" + std::to_string(hook.getID()) + " is already registered";
	  retval = false;
	}
      }
      if (retval == true) {
	if (hook.hasName()) {
	  infolog("Registering custom webhook id=%d name=%s to url (%s)",
		  hook.getID(), hook.getName(),  hook.getConfigKey("url"));
	  webhooks.push_back(std::make_shared<CustomWebHook>(hook.getID(), hook.getName(), hook.isActive(), hook.getConfigKeys()));
          addPrometheusWebhookMetric(id, hook.getConfigKey("url"), true);
	}
	else {
	  infolog("Registering webhook id=%d for events [%s] to url (%s)",
		  hook.getID(), hook.getEventsStr(), hook.getConfigKey("url"));
	  webhooks.push_back(std::make_shared<WebHook>(hook.getID(), hook.getEvents(), hook.isActive(), hook.getConfigKeys()));
          addPrometheusWebhookMetric(id, hook.getConfigKey("url"), false);
	}
      }
    }
    return retval;
  }
  bool deleteWebHook(unsigned int wh_id)
  {
    std::lock_guard<std::mutex> lock(mutex);
    bool retval = false;
    for (auto i = webhooks.begin(); i!=webhooks.end(); ) {
      if ((*i)->getID() == wh_id) {
	i = webhooks.erase(i);
	retval = true;
	break;
      }
      else
	++i;
    }
    return retval;
  }
  std::weak_ptr<WebHook> getWebHook(unsigned int wh_id)
  {
    std::lock_guard<std::mutex> lock(mutex);
    for (auto i = webhooks.begin(); i!=webhooks.end(); ++i) {
      if ((*i)->getID() == wh_id)
	return *i;
    }
    return std::weak_ptr<WebHook>();
  }
  std::weak_ptr<WebHook> getWebHook(const std::string& wh_name)
  {
    std::lock_guard<std::mutex> lock(mutex);
    for (auto i = webhooks.begin(); i!=webhooks.end(); ++i) {
      if ((*i)->getName().compare(wh_name) == 0)
	return *i;
    }
    return std::weak_ptr<WebHook>();
  }
  const std::vector<std::weak_ptr<const WebHook>> getWebHooks() const
  {
    std::lock_guard<std::mutex> lock(mutex);
    std::vector<std::weak_ptr<const WebHook>> retvec;
    
    for (const auto& i : webhooks)
      retvec.push_back(i);
    
    return retvec;
  }
  const std::vector<std::weak_ptr<const WebHook>> getWebHooksForEvent(const std::string& event_name) const
  {
    std::lock_guard<std::mutex> lock(mutex);
    std::vector<std::weak_ptr<const WebHook>> retvec;
    
    for (const auto& i : webhooks) {
      for (const auto& ev : i->getEvents()) {
	if (ev.compare(event_name) == 0) {
	  retvec.push_back(i);
	}
      }
    }
    return retvec;
  }
  void toString(std::string& out) const
  {
    std::lock_guard<std::mutex> lock(mutex);
    Json::array jarray;

    for (auto& i : webhooks) {
      jarray.push_back(i->to_json());
    }
    Json my_array = Json(jarray);
    my_array.dump(out);
  }
  std::string toString() const
  {
    std::string out;
    toString(out);
    return out;
  }
private:
  mutable std::atomic<unsigned int> id;
  std::vector<std::shared_ptr<WebHook>> webhooks;
  mutable std::mutex mutex;
};

struct CurlConnection {
  CurlConnection()
  {
  }
  std::mutex 	cmutex;
  MiniCurlMulti	mcurl;
};

using CurlConns = std::vector<std::shared_ptr<CurlConnection>>;

#define MAX_HOOK_CONN 10
#define NUM_WEBHOOK_THREADS 5
#define QUEUE_SIZE 50000
#define DEFAULT_TIMEOUT_SECS 2

struct WebHookQueueItem
{
  std::string event_name;
  std::shared_ptr<const WebHook> hook;
  std::shared_ptr<std::string> hook_data_p;
};

class WebHookRunner
{
public:
  WebHookRunner();
  void setNumThreads(unsigned int new_num_threads);
  void setMaxConns(unsigned int max_conns);
  void setMaxQueueSize(unsigned int max_queue);
  void setTimeout(uint64_t timeout_seconds);
  // synchronously run the ping command for the hook
  bool pingHook(std::shared_ptr<const WebHook> hook, std::string error_msg);
  // asynchronously run the hook with the supplied data
  void runHook(const std::string& event_name, std::shared_ptr<const WebHook> hook, const std::string& hook_data);
  void runHook(const std::string& event_name, std::shared_ptr<const WebHook> hook, const json11::Json& json_data);
  void startThreads();
protected:
  void _runHookThread(unsigned int num_conns);
  void _addHook(const std::string& event_name, std::shared_ptr<const WebHook> hook, const std::string& hook_data, MiniCurlMulti& mcurl);
  bool _runHooks(const std::vector<WebHookQueueItem>& events,
                 MiniCurlMulti& mcurl);
private:
  std::queue<WebHookQueueItem> queue;
  std::mutex queue_mutex;
  std::condition_variable cv;
  unsigned int max_queue_size = QUEUE_SIZE;
  unsigned int max_hook_conns = MAX_HOOK_CONN;
  unsigned int num_threads = NUM_WEBHOOK_THREADS;
  uint64_t timeout_secs = DEFAULT_TIMEOUT_SECS;
};
