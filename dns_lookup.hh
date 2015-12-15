#pragma once
#include <getdns/getdns.h>
#include <string>
#include <vector>
#include <memory>
#include "iputils.hh"

class WFResolver {
public:
  WFResolver();
  ~WFResolver();
  WFResolver(const WFResolver& obj);
  void add_resolver(const std::string& address, int port);
  void set_request_timeout(uint64_t timeout);
  std::vector<std::string> lookup_address_by_name(const std::string& name, boost::optional<size_t> num_retries);
  std::vector<std::string> lookup_name_by_address(const ComboAddress& ca, boost::optional<size_t> num_retries);
  std::vector<std::string> lookupRBL(const ComboAddress& ca, const std::string& rblname, boost::optional<size_t> num_retries);
protected:
  bool create_dns_context(getdns_context **context);
  std::vector<std::string> do_lookup_address_by_name(getdns_context *context, const std::string& name, size_t num_retries);  
  std::vector<std::string> do_lookup_name_by_address(getdns_context *context, getdns_dict* addr_dict, size_t num_retries=0);
private:
  getdns_list* resolver_list;
  uint64_t req_timeout;
};
