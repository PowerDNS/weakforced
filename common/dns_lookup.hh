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
#include <getdns/getdns.h>
#include <getdns/getdns_extra.h>
#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <atomic>
#include "iputils.hh"

struct GetDNSContext {
  getdns_context* context_ctx;
  std::mutex context_mutex;
};

struct AsyncThreadUserData {
  getdns_dict* response;
  getdns_callback_type_t callback_type;
};

#define DNS_REQUEST_TIMEOUT 1000
#define NUM_GETDNS_CONTEXTS 12

class WFResolver {
public:
  WFResolver();
  ~WFResolver();
  WFResolver(const WFResolver&) = delete;
  WFResolver& operator=(const WFResolver&) = delete;
  void add_resolver(const std::string& address, int port);
  void set_request_timeout(uint64_t timeout);
  void set_num_contexts(unsigned int nc);
  std::vector<std::string> lookup_address_by_name(const std::string& name, boost::optional<size_t> num_retries);
  std::vector<std::string> lookup_name_by_address(const ComboAddress& ca, boost::optional<size_t> num_retries);
  std::vector<std::string> lookupRBL(const ComboAddress& ca, const std::string& rblname, boost::optional<size_t> num_retries);
protected:
  void init_dns_contexts();
  bool create_dns_context(getdns_context **context);
  bool get_dns_context(std::shared_ptr<GetDNSContext>* ret_ctx);
  std::vector<std::string> do_lookup_address_by_name(getdns_context *context, const std::string& name, size_t num_retries);  
  std::vector<std::string> do_lookup_name_by_address(getdns_context *context, getdns_dict* addr_dict, size_t num_retries=0);
  std::vector<std::string> do_lookup_address_by_name_async(getdns_context *context, const std::string& name, size_t num_retries);  
  std::vector<std::string> do_lookup_name_by_address_async(getdns_context *context, getdns_dict* addr_dict, size_t num_retries=0);
private:
  getdns_list* resolver_list;
  std::atomic<uint64_t> req_timeout;
  std::mutex mutx;
  unsigned int num_contexts;
  std::vector<std::shared_ptr<GetDNSContext>> contexts;
  std::atomic<unsigned int> context_index;
};

extern std::mutex resolv_mutx;
extern std::map<std::string, std::shared_ptr<WFResolver>> resolvMap;
