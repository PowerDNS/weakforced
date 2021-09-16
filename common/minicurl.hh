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

#pragma once
#include <map>
#include <string>
#include <sstream>
#include <vector>
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
  void setURLData(const std::string& url, const MiniCurlHeaders& headers);
  std::string getURL(const std::string& url, const MiniCurlHeaders& headers);
  void setCurlOptionLong(int option, long l);
  void setCurlOptionString(int option, const char* s);
  void setTimeout(uint64_t timeout_secs);
  void setPostData(const std::string& url, const std::string& post_body,
                   const MiniCurlHeaders& headers);
  std::string getPostResult() { return d_data; }
  bool postURL(const std::string& url, const std::string& post_body,
	       const MiniCurlHeaders& headers,
	       std::string& error_msg);
  // This version returns the POST result in post_res
  bool postURL(const std::string& url, const std::string& post_body,
	       const MiniCurlHeaders& headers,
	       std::string& post_res,
	       std::string& error_msg);
  CURL* getCurlHandle() { return d_curl; }
  unsigned int getID() { return d_id; }
  std::string getErrorMsg() { return std::string(d_error_buf); }
  void setID(unsigned int id) { d_id = id; }
protected:
  void setCurlHeaders(const MiniCurlHeaders& headers);
  void clearCurlHeaders();
  bool initCurlGlobal();
  bool is2xx(const int& code) const { return code/100 == 2; }
private:
  CURL *d_curl;
  static size_t write_callback(char *ptr, size_t size, size_t nmemb, void *userdata);
  static size_t read_callback(char *buffer, size_t size, size_t nitems, void *userdata);
  std::string d_data;
  std::istringstream d_post_body;
  struct curl_slist* d_header_list = nullptr;
  char d_error_buf[CURL_ERROR_SIZE];
  unsigned int d_id;
};

const unsigned int numMultiCurlConnections=10;

struct mcmPostReturn {
  unsigned int id;
  bool ret;
  std::string result_data;
  std::string error_msg;
};

class MiniCurlMulti : public MiniCurl
{
public:
  MiniCurlMulti();
  MiniCurlMulti(size_t num_connections);
  ~MiniCurlMulti();
  unsigned int getNumConnections() { return d_ccs.size(); }
  bool addPost(unsigned id, const std::string& url,
               const std::string& post_body,
               const MiniCurlHeaders& headers);
  const std::vector<mcmPostReturn> runPost();
  void setTimeout(uint64_t timeout_secs);
protected:
  void initMCurl();
  void finishPost();
  std::vector<MiniCurl>::iterator findMiniCurl(CURL *curl)
  {
    for (auto i = d_ccs.begin(); i != d_ccs.end(); ++i)
      if (i->getCurlHandle() == curl)
        return i;
    return d_ccs.end();
  }
private:
  std::vector<MiniCurl>::iterator d_current;
  std::vector<MiniCurl> d_ccs;
  CURLM* d_mcurl = nullptr;
};

struct curlTLSOptions {
  bool verifyPeer = true;
  bool verifyHost = true;
  std::string caCertBundleFile;
  std::string clientCertFile;
  std::string clientKeyFile;
};