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

#include "minicurl.hh"
#include <time.h>
#include <chrono>
#include <curl/curl.h>
#include <stdarg.h>
#include <string.h>
#include <sstream>
#include "wforce_exception.hh"
#include "dolog.hh"

bool MiniCurl::initCurlGlobal()
{
  // curl_global_init() is guaranteed to be called only once
  static const CURLcode init_curl_global = curl_global_init(CURL_GLOBAL_ALL);
  return init_curl_global == CURLE_OK;
}

MiniCurl::MiniCurl()
{
  bool init_cg = initCurlGlobal();
  
  if (init_cg) {
    d_curl = curl_easy_init();
    if (d_curl) {
      curl_easy_setopt(d_curl, CURLOPT_ERRORBUFFER, d_error_buf);
    }
  }
  else
    throw WforceException("Cannot initialize curl library");
}

MiniCurl::~MiniCurl()
{
  if (d_curl)
    curl_easy_cleanup(d_curl);
}

size_t MiniCurl::write_callback(char *ptr, size_t size, size_t nmemb, void *userdata)
{
  MiniCurl* us = (MiniCurl*)userdata;
  us->d_data.append(ptr, size*nmemb);
  return size*nmemb;
}

void MiniCurl::setURLData(const std::string& url, const MiniCurlHeaders& headers)
{
  if (d_curl) {
    clearCurlHeaders();
    curl_easy_setopt(d_curl, CURLOPT_HTTPGET, 1);
    curl_easy_setopt(d_curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(d_curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(d_curl, CURLOPT_WRITEDATA, this);
    setCurlHeaders(headers);
    d_data.clear();
    d_error_buf[0] = '\0';
  }
}

std::string MiniCurl::getURL(const std::string& url, const MiniCurlHeaders& headers)
{
  if (d_curl) {
    setURLData(url, headers);
    curl_easy_perform(d_curl);
    std::string ret=std::move(d_data);
    d_data.clear();

    return ret;
  }
  return std::string();
}

void MiniCurl::setCurlOption(int option, ...)
{
  if (d_curl) {
    va_list args;

    va_start(args, option);
    (void) curl_easy_setopt(d_curl, static_cast<CURLoption>(option), args);
    va_end(args);
  }
}

void MiniCurl::clearCurlHeaders()
{
  if (d_curl) {
    curl_easy_setopt(d_curl, CURLOPT_HTTPHEADER, NULL);
    if (d_header_list != nullptr) {
      curl_slist_free_all(d_header_list);
      d_header_list = nullptr;
    }
  }
}

void MiniCurl::setCurlHeaders(const MiniCurlHeaders& headers)
{
  if (d_curl) {
    for (auto& header : headers) {
      std::stringstream header_ss;
      header_ss << header.first << ": " << header.second;
      d_header_list = curl_slist_append(d_header_list, header_ss.str().c_str());
    }
    curl_easy_setopt(d_curl, CURLOPT_HTTPHEADER, d_header_list);
  }
}

size_t MiniCurl::read_callback(char *buffer, size_t size, size_t nitems, void *userdata)
{
  std::stringstream* ss = (std::stringstream*)userdata;
  ss->read(buffer, size*nitems);
  auto bytes_read = ss->gcount();
  return bytes_read;
}

// if you want Content-Type other than application/x-www-form-urlencoded you must set Content-Type
// to be what you want in the supplied headers, e.g. application/json
bool MiniCurl::postURL(const std::string& url,
		       const std::string& post_body,
		       const MiniCurlHeaders& headers,
		       std::string& error_msg)
{
  std::string ignore_res;
  return postURL(url, post_body, headers, ignore_res, error_msg);
}

void MiniCurl::setPostData(const std::string& url,
                           const std::string& post_body,
                           const MiniCurlHeaders& headers)
{
  if (d_curl) {
    d_post_body = std::istringstream(post_body);
    clearCurlHeaders();
        
    curl_easy_setopt(d_curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(d_curl, CURLOPT_POST, 1);
    curl_easy_setopt(d_curl, CURLOPT_POSTFIELDSIZE, post_body.length());
    curl_easy_setopt(d_curl, CURLOPT_READFUNCTION, read_callback);
    curl_easy_setopt(d_curl, CURLOPT_READDATA, &d_post_body);
    curl_easy_setopt(d_curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(d_curl, CURLOPT_WRITEDATA, this);
    setCurlHeaders(headers);
    d_error_buf[0] = '\0';
    d_data.clear();
  }  
}

bool MiniCurl::postURL(const std::string& url,
		       const std::string& post_body,
		       const MiniCurlHeaders& headers,
		       std::string& post_res,
		       std::string& error_msg)
{
  bool retval = false;

  if (d_curl) {
    setPostData(url, post_body, headers);
    
    auto ret = curl_easy_perform(d_curl);

    post_res = std::move(d_data);
    d_data.clear();
    
    if (ret != CURLE_OK) {
      auto eb_len = strlen(d_error_buf);
      if (eb_len) {
	error_msg = std::string(d_error_buf);
      }
      else {
	error_msg = std::string(curl_easy_strerror(ret));
      }
    }
    else {
      long response_code;
      curl_easy_getinfo(d_curl, CURLINFO_RESPONSE_CODE, &response_code);
      if (response_code != 200) {
	error_msg = std::string("Received non-200 response from webserver: ") + std::to_string(response_code);
      }
      else
	retval = true;
    }
  }
  return retval;
}

// MiniCurlMulti

MiniCurlMulti::MiniCurlMulti() : d_ccs(numMultiCurlConnections)
{
  d_mcurl = curl_multi_init();
  initMCurl();
}

MiniCurlMulti::MiniCurlMulti(size_t num_connections) : d_ccs(num_connections)
{
  d_mcurl = curl_multi_init();
  initMCurl();
}

MiniCurlMulti::~MiniCurlMulti()
{
  if (d_mcurl != nullptr) {
    curl_multi_cleanup(d_mcurl);
  }
}

void MiniCurlMulti::initMCurl()
{
#ifdef CURLPIPE_HTTP1
  curl_multi_setopt(d_mcurl, CURLMOPT_PIPELINING,
                    CURLPIPE_HTTP1 | CURLPIPE_MULTIPLEX);
#else
  curl_multi_setopt(d_mcurl, CURLMOPT_PIPELINING, 1L);  
#endif
#ifdef CURLMOPT_MAX_HOST_CONNECTIONS
  curl_multi_setopt(d_mcurl, CURLMOPT_MAX_HOST_CONNECTIONS, d_ccs.size());
#endif
  d_current = d_ccs.begin();
}

bool MiniCurlMulti::addPost(unsigned int id, const std::string& url,
                            const std::string& post_body,
                            const MiniCurlHeaders& headers)
{
  if (d_mcurl) {
    // return false if we don't have any spare connections
    if (d_current == d_ccs.end())
      return false;
    d_current->setPostData(url, post_body, headers);
    d_current->setID(id);
    curl_multi_add_handle(d_mcurl, d_current->getCurlHandle());
    ++d_current;
    return true;
  }
  return false;
}

const std::vector<mcmPostReturn> MiniCurlMulti::runPost()
{
  std::vector<mcmPostReturn> post_ret;
  if (d_mcurl) {
    int still_running;
    int without_fds = 0;
    do {
      CURLMcode mc;
      int numfds;
      mc = curl_multi_perform(d_mcurl, &still_running);
      if (mc == CURLM_OK) {
        auto start_time = std::chrono::steady_clock::now();
        
        mc = curl_multi_wait(d_mcurl, NULL, 0, 1000, &numfds);

        auto wait_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start_time);
        
        if (mc == CURLM_OK) {
          struct CURLMsg *m;
          int msgq = 0;
          while ((m = curl_multi_info_read(d_mcurl, &msgq)) != NULL) {
            if (m->msg == CURLMSG_DONE) {
              CURL *c = m->easy_handle;
              auto minic = findMiniCurl(c);
              if (minic == d_ccs.end()) {
                throw WforceException("Cannot find curl handle in MiniCurlMulti - something is very wrong");
              }
              struct mcmPostReturn mpr;
              if (m->data.result != CURLE_OK) {
                mpr.ret = false;
                std::string error_msg = minic->getErrorMsg();
                if (error_msg.length())
                  mpr.error_msg = error_msg;
                else
                  mpr.error_msg = std::string(curl_easy_strerror(m->data.result));
              }
              else {
                long response_code;
                curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &response_code);
                if ((response_code < 200) || (response_code > 299)) {
                  mpr.error_msg = std::string("Received non-2XX response from webserver: ") + std::to_string(response_code);
                  mpr.ret = false;
                }
                else {
                  mpr.ret = true;
                  mpr.result_data = minic->getPostResult();
                }
              }
              mpr.id = minic->getID();
              post_ret.emplace_back(std::move(mpr));
            }
          }
          if (!numfds) {
            // Avoid busy looping when there is nothing to do
            // The idea is to busy loop the first couple of times
            // and then backoff exponentially until a max wait is reached
            // Inspired by similar code in curl_easy_perform
            if (wait_time_ms.count() <= 10) {
              without_fds++;
              if (without_fds > 2) {
                int sleep_ms = without_fds < 10 ?
                                             (1 << (without_fds - 1)) : 1000;
                struct timespec ts;
                ts.tv_sec = 0;
                ts.tv_nsec = sleep_ms*1000000;
                nanosleep(&ts, nullptr);
              }
            }
            else {
              /* it wasn't "instant", restart counter */
              without_fds = 0;
            }
          }
          else {
            /* got file descriptor, restart counter */
            without_fds = 0;
          }
        }
        else { // curl_multi_wait failed
          errlog("curl_multi_wait failed, code %d.n, error=%s", mc, curl_multi_strerror(mc));
          break;
        }
      }
      else { // curl_multi_perform failed
        errlog("curl_multi_perform failed, code %d.n, error=%s", mc, curl_multi_strerror(mc));
        break;
      }
    } while (still_running);
  }
  finishPost();
  return post_ret;
}

// we don't destroy the multi-handle as reusing has benefits like caching,
// connection pools etc.
void MiniCurlMulti::finishPost()
{
  for (auto i = d_ccs.begin(); i != d_current; ++i) {
    curl_multi_remove_handle(d_mcurl, i->getCurlHandle());
  }
  // Reset the Easy curl handle iterator
  d_current = d_ccs.begin();
}
