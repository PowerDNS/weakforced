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

std::string MiniCurl::getURL(const std::string& url, const MiniCurlHeaders& headers)
{
  if (d_curl) {
    struct curl_slist* header_list;
    curl_easy_setopt(d_curl, CURLOPT_HTTPGET, 1);
    curl_easy_setopt(d_curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(d_curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(d_curl, CURLOPT_WRITEDATA, this);
    setCurlHeaders(headers, &header_list);
    d_data.clear();
    d_error_buf[0] = '\0';

    curl_easy_perform(d_curl);
    std::string ret=std::move(d_data);
    d_data.clear();

    clearCurlHeaders(header_list);
    return ret;
  }
  return std::string();
}

void MiniCurl::setCurlOption(CURLoption option, ...)
{
  if (d_curl) {
    va_list args;
    va_start(args, option);
    (void) curl_easy_setopt(d_curl, option, args);
    va_end(args);
  }
}

void MiniCurl::clearCurlHeaders(struct curl_slist* header_list)
{
  if (d_curl) {
    curl_easy_setopt(d_curl, CURLOPT_HTTPHEADER, NULL);
    curl_slist_free_all(header_list);
  }
}

void MiniCurl::setCurlHeaders(const MiniCurlHeaders& headers, struct curl_slist** header_listp)
{
  if (d_curl) {
    for (auto& header : headers) {
      std::stringstream header_ss;
      header_ss << header.first << ": " << header.second;
      *header_listp = curl_slist_append(*header_listp, header_ss.str().c_str());
    }
    curl_easy_setopt(d_curl, CURLOPT_HTTPHEADER, *header_listp);
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
  bool retval = false;
  
  if (d_curl) {
    std::stringstream ss(post_body);
    struct curl_slist* header_list = NULL;
    
    curl_easy_setopt(d_curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(d_curl, CURLOPT_POST, 1);
    curl_easy_setopt(d_curl, CURLOPT_READFUNCTION, read_callback);
    curl_easy_setopt(d_curl, CURLOPT_READDATA, &ss);
    curl_easy_setopt(d_curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(d_curl, CURLOPT_WRITEDATA, this);
    setCurlHeaders(headers, &header_list);
    d_error_buf[0] = '\0';
    d_data.clear();
    
    auto ret = curl_easy_perform(d_curl);

    clearCurlHeaders(header_list);
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
