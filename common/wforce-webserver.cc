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

#include "wforce-webserver.hh"
#include "sstuff.hh"
#include "dolog.hh"
#include "perf-stats.hh"
#include "base64.hh"
#include "json11.hpp"
#include "ext/threadname.hh"
#include "prometheus.hh"
#include <time.h>

using std::thread;

void WforceWebserver::setPollTimeout(int timeout_ms)
{
  d_poll_timeout = timeout_ms;
}

void WforceWebserver::setNumWorkerThreads(unsigned int num_workers)
{
  d_num_worker_threads = num_workers;
}

void WforceWebserver::setMaxConns(unsigned int max_conns)
{
  d_max_conns = max_conns;
}

void WforceWebserver::setContentType(const std::string& content_type)
{
  d_content_type = content_type;
}

void WforceWebserver::setACL(const NetmaskGroup& nmg)
{
  d_ACL.setState(nmg);
}

void WforceWebserver::addACL(const std::string& ip)
{
  d_ACL.modify([ip](NetmaskGroup& nmg) { nmg.addMask(ip); });
}

NetmaskGroup WforceWebserver::getACL()
{
  return d_ACL.getCopy();
}

size_t WforceWebserver::getNumConns()
{
  std::lock_guard<std::mutex> lock(d_sock_vec_mutx);
  return d_sock_vec.size();
}

bool WforceWebserver::registerFunc(const std::string& command, HTTPVerb verb, const WforceWSFunc& func)
{
  bool retval = false;
  
  switch (verb) {
  case HTTPVerb::GET:
    {
      auto f = d_get_map.find(command);
      if (f == d_get_map.end()) {
	d_get_map.insert(std::make_pair(command, func));
	retval = true;
      }
      break;
    }
  case HTTPVerb::POST:
    {
      auto f = d_post_map.find(command);
      if (f == d_post_map.end()) {
	d_post_map.insert(std::make_pair(command, func));
	retval = true;
      }
      break;
    }
  case HTTPVerb::PUT:
    {
      auto f = d_put_map.find(command);
      if (f == d_put_map.end()) {
	d_put_map.insert(std::make_pair(command, func));
	retval = true;
      }
      break;
    }
  case HTTPVerb::DELETE:
    {
      auto f = d_delete_map.find(command);
      if (f == d_delete_map.end()) {
	d_delete_map.insert(std::make_pair(command, func));
	retval = true;
      }
      break;
    }
  }
  return retval;
}

void WforceWebserver::connectionThread(WforceWebserver* wws)
{
  using namespace json11;
  const std::string ct_json = "application/json";
  
  setThreadName("wf/web-worker");
  
  while (true) {
    std::shared_ptr<WFConnection> wfc;
    {
      std::unique_lock<std::mutex> lock(wws->d_queue_mutex);
      while (wws->d_queue.size() == 0) {
        wws->d_cv.wait(lock);
      }
      auto qi = wws->d_queue.front();
      wws->d_queue.pop();
      wfc = qi.conn;
    }
    string line;
    string request;
    YaHTTP::Request req;
    bool closeConnection=true;
    bool validRequest = true;
    bool eof = false;
    
    if (!wfc)
      continue;
  
    auto start_time = std::chrono::steady_clock::now();
    auto wait_time = start_time - wfc->q_entry_time;
    auto i_millis = std::chrono::duration_cast<std::chrono::milliseconds>(wait_time);

    YaHTTP::AsyncRequestLoader yarl;
    yarl.initialize(&req);
    int timeout = 5; // XXX make this configurable
    wfc->s.setNonBlocking();
    try {
      bool complete = false;
      while (!complete) {
        int bytes;
        char buf[1024];
        bytes = wfc->s.readWithTimeout(buf, sizeof(buf), timeout);
        if (bytes > 0) {
          string data(buf, bytes);
          complete = yarl.feed(data);
        }
        else if (bytes == 0) {
          eof = true;
          validRequest = false;
          break;
        }
        else {
          validRequest = false;
          break;
        }
      }
      yarl.finalize();
    } catch (YaHTTP::ParseError &e) {
      // request stays incomplete
      infolog("WforceWebserver: Unparseable HTTP request from %s", wfc->remote.toStringWithPort());
      validRequest = false;
    } catch (NetworkError& e) {
      warnlog("WforceWebserver: Network error in web server: %s", e.what());
      validRequest = false;
    }

    if (validRequest) {
      bool keepalive = false;
      vinfolog("WforceWebserver: handling request from %s on fd=%d", wfc->remote.toStringWithPort(), wfc->fd);

      string conn_header = req.headers["Connection"];
      if (conn_header.compare("keep-alive") == 0)
        keepalive = true;

      if (conn_header.compare("close") == 0)
        closeConnection = true;
      else if (req.version > 10 || ((req.version < 11) && (keepalive == true)))
        closeConnection = false;

      string command=req.getvars["command"];

      string callback;

      if(req.getvars.count("callback")) {
        callback=req.getvars["callback"];
        req.getvars.erase("callback");
      }

      req.getvars.erase("_"); // jQuery cache buster

      // Generate HTTP Date format string
      char datebuf[WFORCE_MAX_DATE_STRING_LEN];
      time_t now = time(0);
      struct tm tm = *gmtime(&now);
      auto datelen = std::strftime(datebuf, sizeof datebuf, "%a, %d %b %Y %H:%M:%S GMT", &tm);

      YaHTTP::Response resp;
      if (datelen != 0) {
        resp.headers["Date"] = datebuf;
        resp.headers["Last-Modified"] = datebuf;
      }
      resp.headers["Cache-Control"] = "no-cache";
      resp.headers["Content-Type"] = wws->d_content_type;
      if (closeConnection)
        resp.headers["Connection"] = "close";
      else if (keepalive == true)
        resp.headers["Connection"] = "keep-alive";
      resp.version = req.version;
      resp.url = req.url;

      string ctype = req.headers["Content-Type"];
      if (!compareAuthorization(req, wfc->password)) {
        errlog("WforceWebserver: HTTP Request \"%s\" from %s: Web Authentication failed", req.url.path, wfc->remote.toStringWithPort());
        resp.status=401;
        std::stringstream ss;
        ss << "{\"status\":\"failure\", \"reason\":" << "\"Unauthorized\"" << "}";
        resp.body=ss.str();
        resp.headers["WWW-Authenticate"] = "basic realm=\"wforce\"";
      }
      else {

        // set the defaults in case we don't find a command
        resp.status = 404;
        resp.body = R"({"status":"failure", "reason":"Command not found"})";

        std::string new_content_type;

        if (req.method=="GET") {
          auto metrics_path = std::string("/metrics");
          // First check if this is a request for prometheus metrics
          if (req.url.path.compare(0, metrics_path.length(), metrics_path) == 0) {
            new_content_type = "text/plain";
            resp.status = 200;
            resp.body = serializePrometheusMetrics();
          }
          else {
            const auto& f = wws->d_get_map.find(command);
            if (f != wws->d_get_map.end()) {
              f->second.d_func_ptr(req, resp, command);
              new_content_type = f->second.d_ret_content_type;
            }
          }
        }
        else if (req.method=="DELETE") {
          const auto& f = wws->d_delete_map.find(command);
          if (f != wws->d_delete_map.end()) {
            f->second.d_func_ptr(req, resp, command);
            new_content_type = f->second.d_ret_content_type;
          }
        }
        else if ((command != "") && (ctype.compare(0, ct_json.length(), ct_json) != 0)) {
          errlog("HTTP Request \"%s\" from %s: Content-Type not application/json", req.url.path, wfc->remote.toStringWithPort());
          resp.status = 415;
          std::stringstream ss;
          ss << "{\"status\":\"failure\", \"reason\":" << "\"Invalid Content-Type - must be application/json\"" << "}";
          resp.body=ss.str();
        }
        else if (req.method=="POST") {
          const auto& f = wws->d_post_map.find(command);
          if (f != wws->d_post_map.end()) {
            f->second.d_func_ptr(req, resp, command);
            new_content_type = f->second.d_ret_content_type;
          }
        }
        else if (req.method=="PUT") {
          const auto& f = wws->d_put_map.find(command);
          if (f != wws->d_put_map.end()) {
            f->second.d_func_ptr(req, resp, command);
            new_content_type = f->second.d_ret_content_type;
          }
        }
        if (!new_content_type.empty()) {
          resp.headers["Content-Type"] = new_content_type;
        }
      }
      if(!callback.empty()) {
        resp.body = callback + "(" + resp.body + ");";
      }

      std::ostringstream ofs;
      ofs << resp;
      string done;
      done=ofs.str();
      try {
        wfc->s.writenWithTimeout(done.c_str(), done.size(), timeout);
      }
      catch (const NetworkError& e) {
        warnlog("WforceWebserver: Network error writing to sock: %s", e.what());
        closeConnection = true;
      }
    }

    if (closeConnection) {
      wfc->closeConnection = true;
    }
    auto end_time = std::chrono::steady_clock::now();
    auto run_time = end_time - start_time;
    i_millis = std::chrono::duration_cast<std::chrono::milliseconds>(run_time);
    // Sometimes we get a POLLIN and then read() returns EOF and so we don't want to
    // increment the stats for that
    if (!eof) {
      addWTWStat(i_millis.count());
      addWTRStat(i_millis.count());
      observePrometheusWQD(std::chrono::duration<float>(wait_time).count());
      observePrometheusWRD(std::chrono::duration<float>(run_time).count());
    }
    wfc->inConnectionThread = false;
  }
}

bool WforceWebserver::compareAuthorization(YaHTTP::Request& req, const string &expected_password)
{
  // validate password
  YaHTTP::strstr_map_t::iterator header = req.headers.find("authorization");
  bool auth_ok = false;
  if (header != req.headers.end() && toLower(header->second).find("basic ") == 0) {
    string cookie = header->second.substr(6);

    string plain;
    B64Decode(cookie, plain);

    vector<string> cparts;
    stringtok(cparts, plain, ":");

    // this gets rid of terminating zeros
    auth_ok = (cparts.size()==2 && (0==strcmp(cparts[1].c_str(), expected_password.c_str())));
  }
  return auth_ok;
}

void WforceWebserver::connectionStatsThread(WforceWebserver* wws)
{
  unsigned int interval = 300; // Log every 300 seconds;

  setThreadName("wf/web-conn-stat");
  
  for (;;) {
    sleep(interval);

    noticelog("Number of active connections: %d, Max active connections: %d", wws->getNumConns(), wws->d_max_conns);
  }
}

#include "poll.h"

void WforceWebserver::pollThread(WforceWebserver* wws)
{
  const int fd_increase = 50; // somewhat arbitrary
  struct pollfd* fds=NULL;
  int max_fd_size = -1;

  setThreadName("wf/web-poll");
  
  for (size_t i=0; i<wws->d_num_worker_threads; ++i) {
    std::thread t([wws] { WforceWebserver::connectionThread(wws); });
    t.detach();
  }

  // Start a thread that logs connection stats periodically
  std::thread t([wws] { WforceWebserver::connectionStatsThread(wws); });
  t.detach();
  
  for (;;) {
    // parse the array of sockets and create a pollfd array
    int num_fds=0;
    {
      std::lock_guard<std::mutex> lock(wws->d_sock_vec_mutx);
      num_fds = wws->d_sock_vec.size();
      setPrometheusActiveConns(num_fds);
      // Only allocate a new pollfd array if it needs to be bigger
      if (num_fds > max_fd_size) {
	if (fds!=NULL)
	  delete[] fds;
	fds = new struct pollfd [num_fds+fd_increase];
	max_fd_size = num_fds+fd_increase;
      }
      if (!fds) {
	errlog("Cannot allocate memory in pollThread()");
	exit(-1);
      }
      int j=0;
      // we want to keep the symmetry between pollfds and sock_vec in terms of number and order of items
      // so rather than remove sockets which are not being processed we just don't set the POLLIN flag for them
      for (WFCArray::iterator i = wws->d_sock_vec.begin(); i != wws->d_sock_vec.end(); ++i, j++) {
	fds[j].fd = (*i)->fd;
	fds[j].events = 0;
	if (!((*i)->inConnectionThread)) {
	  fds[j].events |= POLLIN;
	}
      }
    }

    int res = poll(fds, num_fds, wws->d_poll_timeout);

    if (res < 0) {
      warnlog("poll() system call returned error (%d)", errno);
    }
    else {
      std::lock_guard<std::mutex> vlock(wws->d_sock_vec_mutx);
      for (int i=0; i<num_fds; i++) {
	// set close flag for connections that need closing
	if (fds[i].revents & (POLLHUP | POLLERR | POLLNVAL)) {
	  wws->d_sock_vec[i]->closeConnection = true;
	}
	// process any connections that have activity
	else if (fds[i].revents & POLLIN) {
          {
            std::lock_guard<std::mutex> qlock(wws->d_queue_mutex);
            // If too many active connections, don't give the worker threads any more work
            // Also don't add the connection to the queue more than once
            if ((wws->d_queue.size() < wws->d_max_conns) &&
                (!wws->d_sock_vec[i]->inConnectionThread)) {
              wws->d_sock_vec[i]->inConnectionThread = true;
              wws->d_sock_vec[i]->q_entry_time = std::chrono::steady_clock::now();
              WebserverQueueItem wqi = { wws->d_sock_vec[i] };
              wws->d_queue.push(wqi);
            }
          }
	}
      }
      bool queue_empty = true;
      {
        std::lock_guard<std::mutex> qlock(wws->d_queue_mutex);
        if (wws->d_queue.size() > 0)
          queue_empty = false;
        setPrometheusWebQueueSize(wws->d_queue.size());
      }
      // We want to call notify_all() only when we don't hold the lock
      if (!queue_empty)
        wws->d_cv.notify_all();
      // now erase any connections that are done with
      for (WFCArray::iterator i = wws->d_sock_vec.begin(); i != wws->d_sock_vec.end();) {
	if ((*i)->closeConnection == true) {
	  // this will implicitly close the socket through the Socket class destructor
	  i = wws->d_sock_vec.erase(i);
	}
	else
	  ++i;
      }
    }
  }
}

void WforceWebserver::start(int sock, const ComboAddress& local, const std::string& password, WforceWebserver* wws)
{
  noticelog("WforceWebserver launched on %s", local.toStringWithPort());
  auto localACL=wws->d_ACL.getLocal();

  setThreadName("wf/web-accept");

  // spin up a thread to do the polling on the connections accepted by this thread
  thread t1(WforceWebserver::pollThread, wws);
  t1.detach();

  for(;;) {
    try {
      ComboAddress remote(local);
      int fd = SAccept(sock, remote);
      if(!localACL->match(remote)) {
	close(fd);
	continue;
      }
      {
	std::lock_guard<std::mutex> lock(wws->d_sock_vec_mutx);
	wws->d_sock_vec.push_back(std::make_shared<WFConnection>(fd, remote, password));
      }
    }
    catch(std::exception& e) {
      errlog("WforceWebserver: error accepting new webserver connection: %s", e.what());
    }
  }
}
