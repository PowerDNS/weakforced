pragma once
#include <string>
#include <curl/curl.h> 
// turns out 'CURL' is currently typedef for void which means we can't easily forward declare it

class MiniCurl
{
public:
  MiniCurl();
  ~MiniCurl();
  MiniCurl& operator=(const MiniCurl&) = delete;
  std::string getURL(const std::string& str);
private:
  CURL *d_curl;
  static size_t write_callback(char *ptr, size_t size, size_t nmemb, void *userdata);
  std::string d_data;
};
