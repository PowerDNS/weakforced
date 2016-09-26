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

#pragma once
#include <mutex>
#include <curl/curl.h>
#include "ext/json11/json11.hpp"
#include "ext/ctpl.h"
#include "dolog.hh"
#include "minicurl.hh"

using namespace json11;

using WHConfigMap = std::map<std::string, std::string>;
using WHEvents = std::vector<std::string>;
// Key = event name, value is a pair of config key vectors -
// first is mandatory, second is optional
using WHEventConfig = std::pair<std::vector<std::string>, std::vector<std::string>>;
using WHEventTypes = std::map<std::string, WHEventConfig>;

class WebHook {
public:
  WebHook(unsigned int wh_id, const WHEvents& wh_events, bool wh_active,
	  const WHConfigMap& wh_config_keys) : id(wh_id),  active(wh_active), num_success(0), num_failed(0)  {
    addEvents(wh_events);
    config_keys = wh_config_keys;
  }
  bool operator==(const WebHook& rhs)
  {
    return id == rhs.getID();
  }
  unsigned int getID() const { return id; }
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
    bool duplicate = false;
    if (validEventName(event_name)) {
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
  bool validateConfig(std::string& error_msg) const
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
	return i.second;
      }
    }
    else
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
    for (const auto i : new_cfg) {
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
  Json to_json() const
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
  void incSuccess() const
  {
    std::lock_guard<std::mutex> lock(mutex);
    num_success++;
  }
  unsigned int getSuccess() const
  {
    std::lock_guard<std::mutex> lock(mutex);    
    return num_success;
  }
  void incFailed() const
  {
    std::lock_guard<std::mutex> lock(mutex);
    num_failed++;
  }
  unsigned int getFailed() const
  {
    std::lock_guard<std::mutex> lock(mutex);
    return num_failed;
  }
private:
  unsigned int id;
  std::vector<std::string> events;
  bool active;
  WHConfigMap config_keys;
  mutable unsigned int num_success;
  mutable unsigned int num_failed;
  mutable std::mutex mutex;
  const WHEventTypes event_names = { { "report", {{ "url" }, {"secret"}}},
				     { "allow", {{ "url" }, {"secret", "allow_filter"}}},
				     { "reset", {{ "url" }, {"secret"}}},
				     { "addbl", {{ "url" }, {"secret"}}},
				     { "delbl", {{ "url" }, {"secret"}}},
				     { "expirebl", {{ "url" }, {"secret"}}}};
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
	infolog("Registering webhook id=%d for events [%s] to url (%s)",
		hook.getID(), hook.getEventsStr(), hook.getConfigKey("url"));
	webhooks.push_back(std::make_shared<WebHook>(hook.getID(), hook.getEvents(), hook.isActive(), hook.getConfigKeys()));
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
  std::shared_ptr<WebHook> getWebHook(unsigned int wh_id)
  {
    std::lock_guard<std::mutex> lock(mutex);
    for (auto i = webhooks.begin(); i!=webhooks.end(); ) {
      if ((*i)->getID() == wh_id)
	return *i;
    }
    return nullptr;
  }
  const std::vector<std::shared_ptr<const WebHook>> getWebHooks() const
  {
    std::lock_guard<std::mutex> lock(mutex);
    std::vector<std::shared_ptr<const WebHook>> retvec;
    
    for (const auto& i : webhooks)
      retvec.push_back(i);
    
    return retvec;
  }
  const std::vector<std::shared_ptr<const WebHook>> getWebHooksForEvent(const std::string& event_name) const
  {
    std::lock_guard<std::mutex> lock(mutex);
    std::vector<std::shared_ptr<const WebHook>> retvec;
    
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

using MiniCurlP = std::shared_ptr<MiniCurl>;

struct CurlConnection {
  CurlConnection()
  {
    cmutex = std::make_shared<std::mutex>();
    mcurl = std::make_shared<MiniCurl>();
  }
  std::shared_ptr<std::mutex> 	cmutex;
  MiniCurlP			mcurl;
};

using CurlConnMap = std::map<unsigned int, std::vector<CurlConnection>>;

#define MAX_HOOK_CONN 10
#define NUM_WEBHOOK_THREADS 5

class WebHookRunner
{
public:
  WebHookRunner();
  void setNumThreads(unsigned int num_threads);
  void setMaxConns(unsigned int max_conns);
  // synchronously run the ping command for the hook
  bool pingHook(std::shared_ptr<const WebHook> hook, std::string error_msg);
  // asynchronously run the hook with the supplied data (must be in json format)
  void runHook(const std::string& event_name, std::shared_ptr<const WebHook> hook, const std::string& hook_data);
protected:
  void getConnection(unsigned int hook_id, CurlConnection& out_cc);
  static void releaseConnection(CurlConnection& cc);
  static void _runHookThread(int id, const std::string& event_name, std::shared_ptr<const WebHook> hook, const std::string& hook_data, CurlConnection& cc);
  static bool _runHook(const std::string& event_name, std::shared_ptr<const WebHook> hook, const std::string& hook_data, CurlConnection& cc);
private:
  ctpl::thread_pool p;
  unsigned int max_hook_conns = MAX_HOOK_CONN;
  std::mutex  conn_mutex;
  CurlConnMap conns;
};
