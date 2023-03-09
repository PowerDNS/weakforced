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
#include <memory>
#include <curl/curlver.h>
#if defined(LIBCURL_VERSION_NUM) && LIBCURL_VERSION_NUM >= 0x073200
/* we need this so that 'CURL' is not typedef'd to void,
   which prevents us from wrapping it in a unique_ptr.
   Wrapping in a shared_ptr is fine because of type erasure,
   but it is a bit wasteful. */
#define CURL_STRICTER 1
#endif
#include <curl/curl.h> 
// turns out 'CURL' is currently typedef for void which means we can't easily forward declare it

using MiniCurlHeaders = std::map<std::string, std::string>;

class MiniCurl
{
public:
  MiniCurl();
  virtual ~MiniCurl();
  MiniCurl& operator=(const MiniCurl&) = delete;
  MiniCurl& operator=(const MiniCurl&&) = delete;
  void setURLData(const std::string& url, const MiniCurlHeaders& headers);
  std::string getURL(const std::string& url, const MiniCurlHeaders& headers);
  template <class T> void setCurlOption(int option, T optval) {
    if (d_curl) {
#ifdef CURL_STRICTER
      (void) curl_easy_setopt(d_curl.get(), static_cast<CURLoption>(option), optval);
#else
      (void) curl_easy_setopt(d_curl, static_cast<CURLoption>(option), optval);
#endif
    }
  }
  virtual void setTimeout(uint64_t timeout_secs);
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
  CURL* getCurlHandle();
  unsigned int getID() { return d_id; }
  std::string getErrorMsg() { return std::string(d_error_buf); }
  void setID(unsigned int id) { d_id = id; }
protected:
  void setCurlOpts();
  void setCurlHeaders(const MiniCurlHeaders& headers);
  void clearCurlHeaders();
  bool initCurlGlobal();
  bool is2xx(const int& code) const { return code/100 == 2; }
private:
#ifdef CURL_STRICTER
  std::unique_ptr<CURL, decltype(&curl_easy_cleanup)> d_curl{nullptr, curl_easy_cleanup};
  std::unique_ptr<struct curl_slist, decltype(&curl_slist_free_all)> d_header_list{nullptr, curl_slist_free_all};
#else
  CURL *d_curl{};
  struct curl_slist* d_header_list{};
#endif
  static size_t write_callback(char *ptr, size_t size, size_t nmemb, void *userdata);
  static size_t read_callback(char *buffer, size_t size, size_t nitems, void *userdata);
  std::string d_data;
  std::istringstream d_post_body;

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
  template <class T> void setMCurlOption(int option, T optval) {
    setCurlOption(option, optval);
    for (auto& i : d_ccs) {
      i.setCurlOption(option, optval);
    }
  }
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
#ifdef CURL_STRICTER
  std::unique_ptr<CURLM, decltype(&curl_multi_cleanup)> d_mcurl{nullptr, curl_multi_cleanup};
#else
  CURLM* d_mcurl = nullptr;
#endif
};

struct curlTLSOptions {
  bool verifyPeer = true;
  bool verifyHost = true;
  std::string caCertBundleFile;
  std::string clientCertFile;
  std::string clientKeyFile;
};