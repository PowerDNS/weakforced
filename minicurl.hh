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
#include <map>
#include <string>
#include <curl/curl.h> 
// turns out 'CURL' is currently typedef for void which means we can't easily forward declare it

using MiniCurlHeaders = std::map<std::string, std::string>;

class MiniCurl
{
public:
  MiniCurl();
  ~MiniCurl();
  MiniCurl& operator=(const MiniCurl&) = delete;
  MiniCurl& operator=(const MiniCurl&&) = delete;
  std::string getURL(const std::string& url, const MiniCurlHeaders& headers);
  void setCurlOption(CURLoption option, ...);
  bool postURL(const std::string& url, const std::string& post_body, const MiniCurlHeaders& headers,
	       std::string& error_msg);
protected:
  void setCurlHeaders(const MiniCurlHeaders& headers, struct curl_slist** header_listp);
  void clearCurlHeaders(struct curl_slist* header_list);
private:
  CURL *d_curl;
  static size_t write_callback(char *ptr, size_t size, size_t nmemb, void *userdata);
  static size_t read_callback(char *buffer, size_t size, size_t nitems, void *userdata);
  std::string d_data;
  char d_error_buf[CURL_ERROR_SIZE];
};
