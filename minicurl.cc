#include "minicurl.hh"
#include <curl/curl.h>

MiniCurl::MiniCurl()
{
  d_curl = curl_easy_init();
}

MiniCurl::~MiniCurl()
{
  curl_easy_cleanup(d_curl);
}

size_t MiniCurl::write_callback(char *ptr, size_t size, size_t nmemb, void *userdata)
{
  MiniCurl* us = (MiniCurl*)userdata;
  us->d_data.append(ptr, size*nmemb);
  return size*nmemb;
}

std::string MiniCurl::getURL(const std::string& str)
{
  curl_easy_setopt(d_curl, CURLOPT_URL, str.c_str());
  curl_easy_setopt(d_curl, CURLOPT_WRITEFUNCTION, write_callback);
  curl_easy_setopt(d_curl, CURLOPT_WRITEDATA, this);
  d_data.clear();
  auto res = curl_easy_perform(d_curl);
  std::string ret=d_data;
  d_data.clear();
  return ret;
}
