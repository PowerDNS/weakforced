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

#include <boost/date_time/posix_time/posix_time.hpp>
#include "webhook.hh"
#include "dolog.hh"
#include "hmac.hh"
#include "base64.hh"
#include "ext/threadname.hh"

using namespace boost::posix_time;

WebHookRunner::WebHookRunner()
{
}

void WebHookRunner::setNumThreads(unsigned int num_threads)
{
  this->num_threads = num_threads;
}

void WebHookRunner::startThreads()
{
  for (size_t i=0; i<num_threads; ++i) {
    std::thread t([=] { _runHookThread(max_hook_conns); });
    t.detach();
  }
}

void WebHookRunner::setMaxQueueSize(unsigned int max_queue)
{
  max_queue_size = max_queue;
}

void WebHookRunner::setMaxConns(unsigned int max_conns)
{
  max_hook_conns = max_conns;
}

void WebHookRunner::setTimeout(uint64_t tseconds)
{
  timeout_secs = tseconds;
}

// synchronously run the ping command for the hook
bool WebHookRunner::pingHook(std::shared_ptr<const WebHook> hook, std::string error_msg)
{
  MiniCurlMulti mcm;
  
  _addHook("ping", hook, std::string(), mcm);
  std::vector<WebHookQueueItem> wqi = { {std::string("ping"), hook, std::make_shared<std::string>(std::string())} };
  return _runHooks(wqi, mcm);
}

// asynchronously run the hook with the supplied data
void WebHookRunner::runHook(const std::string& event_name, std::shared_ptr<const WebHook> hook, const std::string& hook_data)
{
  std::string err_msg;

  vdebuglog("runHook: event %s hook id %d", event_name, hook->getID());
  if (!hook->validateConfig(err_msg)) {
    errlog("runHook: Error validating configuration of webhook id=%d for event (%s) [%s]", hook->getID(), event_name, err_msg);
  }
  else {
    WebHookQueueItem wqi = {
      event_name,
      hook,
      std::make_shared<std::string>(hook_data)
    };
    {
      std::lock_guard<std::mutex> lock(queue_mutex);
      if (queue.size() >= max_queue_size) {
        errlog("runHook: Webhook queue at max size (%d) - dropping webhook id=%d for event (%s)", max_queue_size, hook->getID(), event_name);
        return;
      }
      else {
        queue.push(wqi);
      }
      setPrometheusWebhookQueueSize(queue.size());
    }
    cv.notify_one();
  }
}

void WebHookRunner::runHook(const std::string& event_name, std::shared_ptr<const WebHook> hook, const json11::Json& json_data)
{
  if (hook->getConfigKey("kafka") == "true") {
    json11::Json kobj = json11::Json::object{{"records", json11::Json(json11::Json::array{json11::Json(json11::Json::object{{"value", json_data}})})}};
    runHook(event_name, hook, kobj.dump());
  }
  else {
    runHook(event_name, hook, json_data.dump());
  }
}

void WebHookRunner::_runHookThread(unsigned int num_conns)
{
  setThreadName("wf/wh-runhook");
  MiniCurlMulti mcm(num_conns);
  mcm.setTimeout(timeout_secs);
  mcm.setCurlOptionLong(CURLOPT_SSL_VERIFYHOST, verify_host ? 2L : 0L);
  mcm.setCurlOptionLong(CURLOPT_SSL_VERIFYPEER, verify_peer ? 1L : 0L);
  if (caCertBundleFile.length() != 0)
    mcm.setCurlOptionString(CURLOPT_CAINFO, caCertBundleFile.c_str());

  while (true) {
    std::vector<WebHookQueueItem> events;
    {
      std::unique_lock<std::mutex> lock(queue_mutex);
      while (queue.size() == 0) {
        cv.wait(lock);
      }
      for (unsigned int i=0;
           i<num_conns && queue.size() != 0;
           ++i) {
        events.push_back(queue.front());
        queue.pop();
      }
    }
    for (auto i = events.begin(); i != events.end(); ++i) {
      _addHook(i->event_name, i->hook, *(i->hook_data_p), mcm);
    }
    _runHooks(events, mcm);
  }
}

bool WebHookRunner::_runHooks(const std::vector<WebHookQueueItem>& events,
                              MiniCurlMulti& mcurl)
{
  bool ret = true;
  const auto& retvec = mcurl.runPost();

  for (auto i = retvec.begin(); i!=retvec.end(); ++i) {
    std::shared_ptr<const WebHook> hook = nullptr;
    for (auto j = events.begin(); j != events.end(); ++j) {
      if (i->id == j->hook->getID()) {
        hook = j->hook;
        break;
      }
    }
    if (hook != nullptr) {
      if (i->ret != true) {      
        errlog("Webhook id=%d failed to url (%s): [%s]",
               i->id, hook->getConfigKey("url"), i->error_msg);
        hook->incFailed();
        ret = false;
      }
      else {
        vinfolog("Webhook id=%d succeeded to url (%s)",
                 i->id, hook->getConfigKey("url"));
        hook->incSuccess();
      }
    }
  }
  return ret;
}

void WebHookRunner::_addHook(const std::string& event_name, std::shared_ptr<const WebHook> hook, const std::string& hook_data, MiniCurlMulti& mcurl)
{
  // construct the necessary headers
  MiniCurlHeaders mch;

  mch.insert(std::make_pair("X-Wforce-Event", event_name));
  if (hook->hasConfigKey("content-type")) {
    mch.insert(std::make_pair("Content-Type", hook->getConfigKey("content-type")));
  }
  else {
    mch.insert(std::make_pair("Content-Type", "application/json"));
  }
  
  mch.insert(std::make_pair("X-Wforce-HookID", std::to_string(hook->getID())));
  if (hook->hasConfigKey("secret"))
    mch.insert(std::make_pair("X-Wforce-Signature",
                             Base64Encode(calculateHMAC(hook->getConfigKey("secret"),
                                                        hook_data, HashAlgo::SHA256))));
  ptime t(microsec_clock::universal_time());
  std::string b64_hash_id = Base64Encode(calculateHash(to_simple_string(t)+std::to_string(hook->getID())+event_name, HashAlgo::SHA256));
  mch.insert(std::make_pair("X-Wforce-Delivery", b64_hash_id));

  if (hook->hasConfigKey("basic-auth")) {
    mch.insert(std::make_pair("Authorization", "Basic " + Base64Encode(hook->getConfigKey("basic-auth"))));
  }

  if (hook->hasConfigKey("api-key")) {
    mch.insert(std::make_pair("X-API-Key", hook->getConfigKey("api-key")));
  }
  
  vdebuglog("Webhook id=%d starting for event (%s) to url (%s) with delivery id (%s) and hook_data (%s)",
          hook->getID(), event_name, hook->getConfigKey("url"), b64_hash_id, hook_data);

  mcurl.addPost(hook->getID(), hook->getConfigKey("url"), hook_data, mch);
}
