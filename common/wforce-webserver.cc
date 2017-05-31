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

#include "wforce-webserver.hh"
#include "sstuff.hh"
#include "dolog.hh"
#include "perf-stats.hh"
#include "base64.hh"
#include "json11.hpp"

using std::thread;

void WforceWebserver::setPollTimeout(int timeout_ms)
{
  d_poll_timeout = timeout_ms;
}

void WforceWebserver::setNumWorkerThreads(unsigned int num_workers)
{
  d_num_worker_threads = num_workers;
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

bool WforceWebserver::registerFunc(const std::string& command, HTTPVerb verb, WforceWSFunc func)
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

void WforceWebserver::connectionThread(int id, std::shared_ptr<WFConnection> wfc, WforceWebserver* wws)
{
  using namespace json11;
  string line;
  string request;
  YaHTTP::Request req;
  bool keepalive = false;
  bool closeConnection=true;
  bool validRequest = true;

  if (!wfc)
    return;

  auto start_time = std::chrono::steady_clock::now();
  auto wait_time = start_time - wfc->q_entry_time;
  auto i_millis = std::chrono::duration_cast<std::chrono::milliseconds>(wait_time);
  addWTWStat(i_millis.count());

  vinfolog("WforceWebserver: handling request from %s on fd=%d", wfc->remote.toStringWithPort(), wfc->fd);

  YaHTTP::AsyncRequestLoader yarl;
  yarl.initialize(&req);
  int timeout = 5; // XXX make this configurable
  wfc->s.setNonBlocking();
  bool complete=false;
  try {
    while(!complete) {
      int bytes;
      char buf[1024];
      bytes = wfc->s.readWithTimeout(buf, sizeof(buf), timeout);
      if (bytes > 0) {
	string data(buf, bytes);
	complete = yarl.feed(data);
      } else {
	// read error OR EOF
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

    YaHTTP::Response resp;
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
    
      if (req.method=="GET") {
	const auto& f = wws->d_get_map.find(command);
	if (f != wws->d_get_map.end()) {
	  f->second(req, resp, command);
	}
      }
      else if (req.method=="DELETE") {
	const auto& f = wws->d_delete_map.find(command);
	if (f != wws->d_delete_map.end()) {
	  f->second(req, resp, command);
	}
      }
      else if ((command != "") && (ctype.compare("application/json") != 0)) {
	errlog("HTTP Request \"%s\" from %s: Content-Type not application/json", req.url.path, wfc->remote.toStringWithPort());
	resp.status = 415;
	std::stringstream ss;
	ss << "{\"status\":\"failure\", \"reason\":" << "\"Invalid Content-Type - must be application/json\"" << "}";
	resp.body=ss.str();
      }
      else if (req.method=="POST") {
	const auto& f = wws->d_post_map.find(command);
	if (f != wws->d_post_map.end()) {
	  f->second(req, resp, command);
	}
      }
      else if (req.method=="PUT") {
	const auto& f = wws->d_put_map.find(command);
	if (f != wws->d_put_map.end()) {
	  f->second(req, resp, command);
	}
      }
    }
    if(!callback.empty()) {
      resp.body = callback + "(" + resp.body + ");";
    }

    std::ostringstream ofs;
    ofs << resp;
    string done;
    done=ofs.str();
    writen2(wfc->fd, done.c_str(), done.size());
  }

  {
    std::lock_guard<std::mutex> lock(wws->d_sock_vec_mutx);
    if (closeConnection) {
      wfc->closeConnection = true;
    }
    wfc->inConnectionThread = false;
    auto end_time = std::chrono::steady_clock::now();
    auto run_time = end_time - start_time;
    auto i_millis = std::chrono::duration_cast<std::chrono::milliseconds>(run_time);
    addWTRStat(i_millis.count());
    return;
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

#include "poll.h"

void WforceWebserver::pollThread(WforceWebserver* wws)
{
  ctpl::thread_pool p(wws->d_num_worker_threads, 5000);
  const int fd_increase = 50; // somewhat arbitrary
  struct pollfd* fds=NULL;
  int max_fd_size = -1;

  for (;;) {
    // parse the array of sockets and create a pollfd array
    int num_fds=0;
    {
      std::lock_guard<std::mutex> lock(wws->d_sock_vec_mutx);
      num_fds = wws->d_sock_vec.size();
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
      std::lock_guard<std::mutex> lock(wws->d_sock_vec_mutx);
      for (int i=0; i<num_fds; i++) {
	// set close flag for connections that need closing
	if (fds[i].revents & (POLLHUP | POLLERR | POLLNVAL)) {
	  wws->d_sock_vec[i]->closeConnection = true;
	}
	// process any connections that have activity
	else if (fds[i].revents & POLLIN) {
	  wws->d_sock_vec[i]->inConnectionThread = true;
	  wws->d_sock_vec[i]->q_entry_time = std::chrono::steady_clock::now();
	  p.push(WforceWebserver::connectionThread, wws->d_sock_vec[i], wws);
	}
      }
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
  warnlog("WforceWebserver launched on %s", local.toStringWithPort());
  auto localACL=wws->d_ACL.getLocal();

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
