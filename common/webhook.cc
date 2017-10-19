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

using namespace boost::posix_time;

WebHookRunner::WebHookRunner()
{
  curl_global_init(CURL_GLOBAL_ALL);
}

void WebHookRunner::setNumThreads(unsigned int num_threads)
{
  this->num_threads = num_threads;
}

void WebHookRunner::startThreads()
{
  for (size_t i=0; i<num_threads; ++i) {
    std::thread t([=] { _runHookThread(); });
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

// synchronously run the ping command for the hook
bool WebHookRunner::pingHook(std::shared_ptr<const WebHook> hook, std::string error_msg)
{
  auto cc = getConnection(hook); // this will only return once it has a connection

  if (auto ccs = cc.lock())
    return _runHook("ping", hook, std::string(), ccs);
  else
    return(false);
}

// asynchronously run the hook with the supplied data (must be a string in json format)
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
    }
    cv.notify_all();
  }
}

std::weak_ptr<CurlConnection> WebHookRunner::getConnection(std::shared_ptr<const WebHook> hook)
{
  int hook_id = hook->getID();
  unsigned int num_conns = max_hook_conns;

  if (hook->hasConfigKey("num_conns")) {
    try {
      num_conns = abs(std::stoi(hook->getConfigKey("num_conns")));
    }
    catch (const std::out_of_range& oor) {
      errlog("Webhook config key num_conns is not an integer value (%s) for WebHook (%s)", hook->getConfigKey("num_conns"), hook->getName());
    }
  }
 
  std::lock_guard<std::mutex> lock(conn_mutex);
  return (_getConnection(hook_id, num_conns));
}

std::weak_ptr<CurlConnection> WebHookRunner::_getConnection(unsigned int hook_id, unsigned int num_connections)
{ 
  auto ci = conns.find(hook_id);
  if (ci != conns.end()) { 
    // return a random connection
    int i = std::rand() % ci->second.size();
    return (ci->second.at(i));
  }
  else {
    std::vector<std::shared_ptr<CurlConnection>> vec;
    
    for (unsigned int i=0; i< num_connections; i++) {
      std::shared_ptr<CurlConnection> cc = std::make_shared<CurlConnection>();
      vec.push_back(cc);
    }
    conns.insert(std::make_pair(hook_id, vec));
    return (_getConnection(hook_id, num_connections));
  }
}

void WebHookRunner::_runHookThread()
{
  while (true) {
    WebHookQueueItem wqi;
    {
      std::unique_lock<std::mutex> lock(queue_mutex);
      while (queue.size() == 0) {
        cv.wait(lock);
      }
      wqi = queue.front();
      queue.pop();
    }
    auto ccs = getConnection(wqi.hook);
    if (auto cc = ccs.lock()) {
      _runHook(wqi.event_name, wqi.hook, *(wqi.hook_data_p), cc);
    }
  }
}

bool WebHookRunner::_runHook(const std::string& event_name, std::shared_ptr<const WebHook> hook, const std::string& hook_data, std::shared_ptr<CurlConnection> cc)
{
  // construct the necessary headers
  MiniCurlHeaders mch;
  std::string error_msg;

  if (cc == nullptr) {
    errlog("_runHook: Curl Connection is nullptr, returning");
    return false;
  }
  
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
  
  vdebuglog("Webhook id=%d starting for event (%s) to url (%s) with delivery id (%s) and hook_data (%s)",
          hook->getID(), event_name, hook->getConfigKey("url"), b64_hash_id, hook_data);

  bool ret;
  {
    std::lock_guard<std::mutex> lock(cc->cmutex);
    ret = cc->mcurl.postURL(hook->getConfigKey("url"), hook_data, mch, error_msg);
  }

  if (ret != true) {
    errlog("Webhook id=%d failed for event (%s) to url (%s) with delivery id (%s) [%s]",
           hook->getID(), event_name, hook->getConfigKey("url"), b64_hash_id, error_msg);
    hook->incFailed();
  }
  else {
    vinfolog("Webhook id=%d succeeded for event (%s) to url (%s) with delivery id (%s)",
             hook->getID(), event_name, hook->getConfigKey("url"), b64_hash_id);
    hook->incSuccess();
  }
  return ret;
}
