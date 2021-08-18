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
#include "prometheus.hh"
#include "drogon/drogon.h"
#include <ctime>

using std::thread;

void WforceWebserver::setNumWorkerThreads(unsigned int num_workers)
{
  d_num_worker_threads = num_workers;
}

void WforceWebserver::setMaxConns(unsigned int max_conns)
{
  drogon::app().setMaxConnectionNum(max_conns);
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
  return 0; // XXX - investigate if this can be retrieved from drogon
}

bool WforceWebserver::registerFunc(const std::string& command, HTTPVerb verb, const WforceWSFunc& wsf)
{
  bool retval = true;
  drogon::HttpMethod method = drogon::Get;

  switch (verb) {
    case HTTPVerb::GET: {
      method = drogon::Get;
      auto f = d_get_map.find(command);
      if (f == d_get_map.end()) {
        d_get_map.insert(std::make_pair(command, wsf));
        retval = true;
      }
      break;
    }
    case HTTPVerb::POST: {
      method = drogon::Post;
      auto f = d_post_map.find(command);
      if (f == d_post_map.end()) {
        d_post_map.insert(std::make_pair(command, wsf));
        retval = true;
      }
      break;
    }
    case HTTPVerb::PUT: {
      method = drogon::Put;
      auto f = d_put_map.find(command);
      if (f == d_put_map.end()) {
        d_put_map.insert(std::make_pair(command, wsf));
        retval = true;
      }
      break;
    }
    case HTTPVerb::DELETE: {
      method = drogon::Delete;
      auto f = d_delete_map.find(command);
      if (f == d_delete_map.end()) {
        d_delete_map.insert(std::make_pair(command, wsf));
        retval = true;
      }
      break;
    }
    default:
      retval = false;
      break;
  }
  if (retval) {
    std::string command_str = "/command/" + command;
    drogon::app().registerHandlerViaRegex(command_str,
                                          [this, wsf, command](const drogon::HttpRequestPtr& req,
                                                               std::function<void(const drogon::HttpResponsePtr&)>
                                                               && callback) {
                                            auto start_time = std::chrono::steady_clock::now();
                                            auto resp = drogon::HttpResponse::newHttpResponse();
                                            resp->setContentTypeCode(wsf.d_ret_content_type);
                                            wsf.d_func_ptr(req, command, resp);
                                            callback(resp);
                                            updateWTR(start_time);
                                          },
                                          {method, "ACLFilter", "LoginFilter"});
  }
  return retval;
}

void WforceWebserver::addListener(const std::string& ip, unsigned int port, bool use_ssl, const std::string& cert_file,
                                  const std::string& private_key, bool enable_oldtls,
                                  std::vector<std::pair<std::string, std::string>> opts)
{
  infolog("Adding webserver listener on %s:%d with SSL=%d, cert_file=%s, key_file=%s", ip, port, use_ssl, cert_file, private_key);
  drogon::app().addListener(ip, port, use_ssl, cert_file, private_key, enable_oldtls, opts);
}

bool LoginFilter::compareAuthorization(const std::string& auth_header, const std::string& expected_password)
{
  // validate password
  bool auth_ok = false;
  if (toLower(auth_header).find("basic ") == 0) {
    string cookie = auth_header.substr(6);

    string plain;
    B64Decode(cookie, plain);

    vector<string> cparts;
    stringtok(cparts, plain, ":");

    // this gets rid of terminating zeros
    auth_ok = (cparts.size() == 2 && (0 == strcmp(cparts[1].c_str(), expected_password.c_str())));
  }
  return auth_ok;
}

void LoginFilter::doFilter(const drogon::HttpRequestPtr& req,
                           drogon::FilterCallback&& fcb,
                           drogon::FilterChainCallback&& fccb)
{
  std::string auth_header = req->getHeader("authorization");
  if (auth_header != "") {
    if (compareAuthorization(auth_header, d_password)) {
      fccb();
      return;
    }
  }
  errlog("WforceWebserver: HTTP(S) Request \"%s\" from %s: Web Authentication failed", req->getPath(),
         req->getPeerAddr().toIpPort());
  auto res = drogon::HttpResponse::newHttpResponse();
  res->setStatusCode(drogon::k401Unauthorized);
  std::stringstream ss;
  ss << "{\"status\":\"failure\", \"reason\":" << "\"Unauthorized\"" << "}";
  res->setBody(ss.str());
  res->addHeader("WWW-Authenticate", "basic realm=\"wforce\"");
  fcb(res);
}

void ACLFilter::doFilter(const drogon::HttpRequestPtr& req,
                         drogon::FilterCallback&& fcb,
                         drogon::FilterChainCallback&& fccb)
{
  auto peer_addr = req->getPeerAddr();
  if (d_ACL->match(ComboAddress(peer_addr.getSockAddr(), sizeof(struct sockaddr_in6)))) {
    fccb();
    return;
  }
  else {
    auto res = drogon::HttpResponse::newNotFoundResponse();
    res->setCloseConnection(true);
    fcb(res);
  }
}