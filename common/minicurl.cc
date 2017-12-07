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
#include <curl/curl.h>
#include <stdarg.h>
#include <string.h>
#include <sstream>
#include "dolog.hh"

MiniCurl::MiniCurl()
{
  d_curl = curl_easy_init();
  if (d_curl) {
    curl_easy_setopt(d_curl, CURLOPT_ERRORBUFFER, d_error_buf);
  }
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
    d_post_body = std::stringstream(post_body);
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
