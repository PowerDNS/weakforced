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
#include "wforce.hh"
#include "ext/json11/json11.hpp"
#include "ext/ctpl.h"
#include "dolog.hh"

using namespace json11;

typedef std::map<std::string, std::string> WHConfigMap;
typedef std::vector<std::string> WHEvents;
// Key = event name, value is a pair of config key vectors -
// first is mandatory, second is optional
typedef std::pair<std::vector<std::string>, std::vector<std::string>> WHEventConfig;
typedef std::map<std::string, WHEventConfig> WHEventTypes;

static const WHEventTypes event_names = { { "report", {{ "url" }, {"secret"}}},
					  { "allow", {{ "url" }, {"secret", "allow_filter"}}},
					  { "reset", {{ "url" }, {"secret"}}},
					  { "addbl", {{ "url" }, {"secret"}}},
					  { "delbl", {{ "url" }, {"secret"}}},
					  { "expirebl", {{ "url" }, {"secret"}}}};


class WebHook {
public:
  WebHook(unsigned int wh_id, const WHEvents& wh_events, bool wh_active,
	  const WHConfigMap& wh_config_keys) : id(wh_id)
  {
    events = wh_events;
    active = wh_active;
    config_keys = wh_config_keys;
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
  const WHEvents& getEvents() { return events; } const
  std::string getEventsStr() const 
  {
    std::stringstream ss;
    for (auto& i : events) {
      ss << i << " ";
    }
    return ss.str();
  }
  const WHEvents& setEvents(const WHEvents& new_events) { events = new_events; return events; }
  const WHEvents& addEvents(const WHEvents& new_events)
  {
    for (auto& i : new_events) {
      addEvent(i);
    }
    return events;
  }
  const WHEvents& addEvent(const std::string& event_name)
  {
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
  const WHEvents& deleteEvents(const WHEvents& del_events)
  {
    for (auto& i : del_events) {
      deleteEvent(i);
    }
    return events;
  }
  const WHEvents& deleteEvent(const std::string& event_name)
  {
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
    static const WHEventConfig null_config;

    if (validEventName(event_name)) {
      for (auto& i : event_names) {
	return i.second;
      }
    }
    else
      return null_config;
  }
  const WHConfigMap& getConfigKeys() { return config_keys; }
  std::string getConfigKey(const std::string& key) const {
    auto i = config_keys.find(key);
    return i->second;
  }
  const WHConfigMap& setConfigKeys(const WHConfigMap& new_cfg) { config_keys = new_cfg; return config_keys; }
  const WHConfigMap& addConfigKeys(const WHConfigMap& new_cfg)
  {
    for (const auto i : new_cfg) {
      config_keys.insert(std::make_pair(i.first, i.second));
    }
    return config_keys;
  }
  const WHConfigMap& addConfigKey(const std::string& key, const std::string& val)
  {
    config_keys.insert(std::make_pair(key, val));
    return config_keys;
  }
  const WHConfigMap& deleteConfigKey(const std::string& key)
  {
    config_keys.erase(key);
    return config_keys;
  }
  bool isActive() const { return active; }
  void setActive(bool isActive) { active = isActive; }
  void toString(std::string& out) const
  {
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
    Json::array jevents;
    Json my_object = Json::object {
      { "id", (int)id },
      { "events", events },
      { "config", config_keys }
    };
    return my_object;
  }
private:
  unsigned int id;
  std::vector<std::string> events;
  bool active;
  WHConfigMap config_keys;
};

class WebHookDB
{
public:
  WebHookDB():id(0)
  {
  }
  unsigned int getNewID() { return id++; }
  bool addWebHook(const WebHook& hook, std::string&error_msg)
  {
    if (!hook.validateConfig(error_msg))
      return false;

    infolog("Registering webhook id= %d for events [%s] to url (%s)",
	    hook.getID(), hook.getEventsStr(), hook.getConfigKey("url"));
    
    webhooks.push_back(hook);
    return true;
  }
  bool deleteWebHook(unsigned int wh_id)
  {
    bool retval = false;
    for (auto i = webhooks.begin(); i!=webhooks.end(); ) {
      if (i->getID() == wh_id) {
	i = webhooks.erase(i);
	retval = true;
	break;
      }
      else
	++i;
    }
    return retval;
  }
  const std::vector<WebHook>& getWebHooks()
  {
    return webhooks;
  }
  const std::vector<WebHook> getWebHooksForEvent(const std::string& event_name)
  {
    std::vector<WebHook> retvec;
    
    for (auto& i : webhooks) {
      for (auto& ev : i.getEvents()) {
	if (ev.compare(event_name) == 0) {
	  retvec.push_back(i);
	}
      }
    }
    return retvec;
  }
  void toString(std::string& out)
  {
    Json::array jarray;

    for (auto& i : webhooks) {
      jarray.push_back(i.to_json());
    }
    Json my_array = Json(jarray);
    my_array.dump(out);
  }
  std::string toString()
  {
    std::string out;
    toString(out);
    return out;
  }
private:
  unsigned int id;
  std::vector<WebHook> webhooks;
};

struct CurlConnection {
  CurlConnection()
  {
    cmutex = std::make_shared<std::mutex>();
    curl = curl_easy_init();
  }
  ~CurlConnection()
  {
    curl_easy_cleanup(curl);
  }
  std::shared_ptr<std::mutex> 	cmutex;
  CURL*				curl;
};

typedef std::map<unsigned int, std::vector<CurlConnection>> CurlConnMap;

#define MAX_HOOK_CONN 10

class WebHookRunner
{
public:
  WebHookRunner(unsigned int num_threads);
  ~WebHookRunner();
  void setNumThreads(unsigned int num_threads);
  void setMaxConns(unsigned int max_conns);
  // synchronously run the ping command for the hook
  bool pingHook(const WebHook& hook, std::string error_msg);
  // asynchronously run the hook with the supplied data (must be in json format)
  void runHook(const std::string& event_name, const WebHook& hook, const Json& hook_json);
protected:
  void getConnection(unsigned int hook_id, CURL** curlp, std::shared_ptr<std::mutex>& out_mutex);
  static void releaseConnection(std::shared_ptr<std::mutex> conn_mutex);
  static void _runHook(int id, const std::string& event_name, const WebHook& hook, const Json& hook_json, std::shared_ptr<std::mutex> conn_mutex);
private:
  ctpl::thread_pool p;
  unsigned int max_hook_conns = MAX_HOOK_CONN;
  std::mutex  conn_mutex;
  CurlConnMap conns;
};
