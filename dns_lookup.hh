#pragma once
#include <getdns/getdns.h>
#include <string>
#include <vector>
#include <memory>
#include <mutex>

#include "iputils.hh"

struct GetDNSContext {
  getdns_context* context_ctx;
  std::shared_ptr<std::mutex> context_mutex;
};

#define DNS_REQUEST_TIMEOUT 1000
#define NUM_GETDNS_CONTEXTS 12

class WFResolver {
public:
  WFResolver();
  ~WFResolver();
  WFResolver(const WFResolver& obj);
  void add_resolver(const std::string& address, int port);
  void set_request_timeout(uint64_t timeout);
  void set_num_contexts(unsigned int nc);
  std::vector<std::string> lookup_address_by_name(const std::string& name, boost::optional<size_t> num_retries);
  std::vector<std::string> lookup_name_by_address(const ComboAddress& ca, boost::optional<size_t> num_retries);
  std::vector<std::string> lookupRBL(const ComboAddress& ca, const std::string& rblname, boost::optional<size_t> num_retries);
protected:
  void init_dns_contexts();
  bool create_dns_context(getdns_context **context);
  bool get_dns_context(GetDNSContext& ret_ctx);
  std::vector<std::string> do_lookup_address_by_name(getdns_context *context, const std::string& name, size_t num_retries);  
  std::vector<std::string> do_lookup_name_by_address(getdns_context *context, getdns_dict* addr_dict, size_t num_retries=0);
private:
  getdns_list* resolver_list;
  uint64_t req_timeout;
  std::shared_ptr<std::mutex> mutxp;
  unsigned int num_contexts;
  std::shared_ptr<std::vector<GetDNSContext>> contextsp;
  std::shared_ptr<unsigned int> context_indexp;
};

extern std::map<std::string, std::shared_ptr<WFResolver>> resolvMap;
