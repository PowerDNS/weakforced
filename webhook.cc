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

#include "webhook.hh"
#include "dolog.hh"

WebHookRunner::WebHookRunner(unsigned int num_threads):p(num_threads, 1000)
{
  curl_global_init(CURL_GLOBAL_ALL);
}

WebHookRunner::~WebHookRunner()
{
  for (auto& ci : conns) {
    for (auto& i : ci.second) {
      curl_easy_cleanup(i.curl);
    }
  }
}

void WebHookRunner::setNumThreads(unsigned int num_threads)
{
  p.resize(num_threads);
}

void WebHookRunner::setMaxConns(unsigned int max_conns)
{
  max_hook_conns = max_conns;
}

// synchronously run the ping command for the hook
bool WebHookRunner::pingHook(const WebHook& hook, std::string error_msg)
{
  std::shared_ptr<std::mutex> conn_mutex;
  CURL* curl;
  getConnection(hook.getID(), &curl, conn_mutex); // this will only return once it has a connection

  // do HTTP stuff here
    
  releaseConnection(conn_mutex);
  return true;
}

// asynchronously run the hook with the supplied data (must be in json format)
void WebHookRunner::runHook(const std::string& event_name, const WebHook& hook, const Json& hook_json)
{
  std::shared_ptr<std::mutex> conn_mutex;
  CURL* curl;
  getConnection(hook.getID(), &curl, conn_mutex); // this will only return once it has a connection
  // the called function is responsible for releasing the mutex
  p.push(_runHook, event_name, hook, hook_json, conn_mutex); 
}

void WebHookRunner::getConnection(unsigned int hook_id, CURL** curlp, std::shared_ptr<std::mutex>& out_mutex)
{
  std::lock_guard<std::mutex> lock(conn_mutex);
    
  auto ci = conns.find(hook_id);
  if (ci != conns.end()) { // we already have some connections, maybe we can reuse
    // first we try to get an existing connection
    for (auto& i : ci->second) {
      if (i.cmutex->try_lock()) {
	*curlp = i.curl;
	out_mutex = i.cmutex;
	return;
      }
      else
	continue;
    }
    // ok, nothing doing so we create a new connection if we can
    unsigned int num_conns = ci->second.size();
    if (num_conns < max_hook_conns) {
      CurlConnection cc = CurlConnection();
      cc.cmutex->lock();
      *curlp = cc.curl;
      out_mutex = cc.cmutex;
      ci->second.push_back(cc);
    }
    else {
      // ok so we couldn't create a new connection so let's just wait for an
      // existing connection to become available
      // XXX if very busy, we'll have a bunch of threads trying to get the first lock
      for (auto& i : ci->second) {
	i.cmutex->lock();
	*curlp = i.curl;
	out_mutex = i.cmutex;
	return;
      }
    }
  }
  else { // first time for this hook id
    std::vector<CurlConnection> vec;
    CurlConnection cc = CurlConnection();
    cc.cmutex->lock();
    *curlp = cc.curl;
    out_mutex = cc.cmutex;
    vec.push_back(cc);
    conns.insert(std::make_pair(hook_id, vec));
  }
}

void WebHookRunner::releaseConnection(std::shared_ptr<std::mutex> conn_mutex)
{
  conn_mutex->unlock();
}

void WebHookRunner::_runHook(int id, const std::string& event_name, const WebHook& hook, const Json& hook_json, std::shared_ptr<std::mutex> conn_mutex)
{
  noticelog("Webhook id=%d starting for event (%s) to url (%s)",
	    hook.getID(), event_name, hook.getConfigKey("url"));
  releaseConnection(conn_mutex);
}
