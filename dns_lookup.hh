#pragma once
#include <getdns/getdns.h>
#include <string>
#include <vector>
#include <memory>

class WFResolver {
public:
  WFResolver();
  ~WFResolver();
  WFResolver(const WFResolver& obj);
  void add_resolver(const std::string& address, int port);
  std::vector<std::string> lookup_address_by_name(const std::string& name);
protected:
  bool create_dns_context(getdns_context **context);
private:
  getdns_list* resolver_list;
};
