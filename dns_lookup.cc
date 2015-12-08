#include "config.h"
#ifdef HAVE_GETDNS
#include "dns_lookup.hh"
#include "misc.hh"
#include "iputils.hh"
#include <sstream>
#include <iostream>
#include <assert.h>

#define GETDNS_STR_IPV4 "IPv4"
#define GETDNS_STR_IPV6 "IPv6"
#define GETDNS_STR_ADDRESS_TYPE "address_type"
#define GETDNS_STR_ADDRESS_DATA "address_data"
#define GETDNS_STR_PORT "port"

WFResolver::WFResolver() 
{ 
  resolver_list = getdns_list_create(); 
}

WFResolver::~WFResolver() 
{ 
}

WFResolver::WFResolver(const WFResolver& obj) 
{ 
  resolver_list = obj.resolver_list; 
}

void setAddressDict(getdns_dict* dict, const ComboAddress& ca)
{
  getdns_bindata address_data;
  getdns_bindata addrtype;
  
  addrtype.size = 4;
  addrtype.data = ca.isIpv4() ? (uint8_t*)GETDNS_STR_IPV4 : (uint8_t*)GETDNS_STR_IPV6;
  address_data.size = ca.isIpv4() ? 4 : 16;
  address_data.data = ca.isIpv4() ? (uint8_t*)&(ca.sin4.sin_addr.s_addr) : (uint8_t*)ca.sin6.sin6_addr.s6_addr;

  if (dict) {
    getdns_dict_set_bindata(dict, GETDNS_STR_ADDRESS_TYPE, &addrtype);
    getdns_dict_set_bindata(dict, GETDNS_STR_ADDRESS_DATA, &address_data);
  }
}

void WFResolver::add_resolver(const std::string& address, int port)
{
  getdns_dict *resolver_dict = getdns_dict_create();

  ComboAddress ca = ComboAddress(address);

   if (resolver_dict && resolver_list) {
     setAddressDict(resolver_dict, ca);
     getdns_dict_set_int(resolver_dict, GETDNS_STR_PORT, port);
     size_t list_index=0;
     getdns_list_get_length(resolver_list, &list_index);
     getdns_list_set_dict(resolver_list, list_index, resolver_dict);
   }	
   getdns_dict_destroy(resolver_dict);
}

bool WFResolver::create_dns_context(getdns_context **context)
{
  getdns_namespace_t d_namespace = GETDNS_NAMESPACE_DNS;

  // we don't want the set_from_os=1 because we want stub resolver behavior
  if (context && (getdns_context_create(context, 0) == GETDNS_RETURN_GOOD)) {
    if ((getdns_context_set_context_update_callback(*context, NULL)) ||
	(getdns_context_set_resolution_type(*context, GETDNS_RESOLUTION_STUB)) ||
	(getdns_context_set_namespaces(*context, (size_t)1, &d_namespace)) ||
	(getdns_context_set_dns_transport(*context, GETDNS_TRANSPORT_UDP_FIRST_AND_FALL_BACK_TO_TCP)))
      return false;

    if (*context && resolver_list) {
      getdns_return_t r;
      if ((r = getdns_context_set_upstream_recursive_servers(*context, resolver_list)) == GETDNS_RETURN_GOOD) {
	return true;
      }
    }
  }
  return false;
}

const char hexDigits[] = "0123456789abcdef";

std::vector<std::string> WFResolver::lookupRBL(const ComboAddress& ca, const std::string& rblname)
{
  std::stringstream ss;
  if (ca.isIpv4()) {
    for (int i=3; i>=0; i--) {
      uint8_t bite = (ca.sin4.sin_addr.s_addr >> (i*8)) & 0x000000ff;
      ss << +bite << "."; 
    }
    ss << rblname;
    return(lookup_address_by_name(ss.str()));
  }
  else {
    for (int i=15; i>=0; i--) {
      uint8_t lownib = ca.sin6.sin6_addr.s6_addr[i] & 0x0f;
      uint8_t highnib = (ca.sin6.sin6_addr.s6_addr[i] >> 4) & 0x0f;
      ss << hexDigits[lownib] << "." << hexDigits[highnib] << ".";
    }
    ss << rblname;
    return(lookup_address_by_name(ss.str()));
  }
}

std::vector<std::string> WFResolver::lookup_address_by_name(const std::string& name)
{
  getdns_context *context;
  getdns_dict *response;
  std::vector<std::string> retvec;

  // if we can setup the context we can do the lookup
  if (create_dns_context(&context)) {
    if (!getdns_address_sync(context, name.c_str(), NULL, &response)) {
      uint32_t status;
      if (!getdns_dict_get_int(response, "status", &status)) {
	if (status == GETDNS_RESPSTATUS_GOOD) {
	  getdns_list *resplist;
	  if (!getdns_dict_get_list(response, "just_address_answers", &resplist)) {
	    size_t listlen;
	    if (!getdns_list_get_length(resplist, &listlen)) {
	      for (size_t i=0; i < listlen; i++) {
		getdns_dict *addrdict;
		if (!getdns_list_get_dict(resplist, i, &addrdict)) {
		  getdns_bindata *address_data;
		  if (!getdns_dict_get_bindata(addrdict, "address_data", &address_data)) {
		    char* addr = getdns_display_ip_address(address_data);
		    if (addr)
		      retvec.push_back(std::string(addr));
		    free(addr);
		  }
		}
	      }
	    }
	  }
	}
      }
      getdns_dict_destroy(response);
    }
    getdns_context_destroy(context);
  }
  return retvec;
}

std::vector<std::string> WFResolver::lookup_name_by_address(const ComboAddress& ca)
{
  getdns_context *context;
  getdns_dict *response, *address;
  std::vector<std::string> retvec;

  address = getdns_dict_create();
  
  // if we can setup the context we can do the lookup
  if (address && create_dns_context(&context)) {
    setAddressDict(address, ca);
    if (!getdns_hostname_sync(context, address, NULL, &response)) {
      getdns_list* answer;
      size_t n_answers;
      getdns_return_t r;
      
      if (getdns_dict_get_list(response, "/replies_tree/0/answer", &answer)) {
	// do nothing
      }
      else if (getdns_list_get_length(answer, &n_answers)) {
	// do nothing
      }	
      else for (size_t i=0; i<n_answers && r == GETDNS_RETURN_GOOD; i++) {
	  getdns_dict *rr;
	  getdns_bindata *dname;
	  char* dname_str;
	  
	  if ((r=getdns_list_get_dict(answer, i, &rr))) {
	  }
	  else if (getdns_dict_get_bindata(rr, "/rdata/ptrdname", &dname)) {
            continue; /* Not a PTR */
	  }
	  else if ((r=getdns_convert_dns_name_to_fqdn(dname, &dname_str))) {
	  }
	  else {
	    retvec.push_back(std::string(dname_str));
            free(dname_str);
	  }
	}
      getdns_dict_destroy(response);
    }
    getdns_context_destroy(context);
  }
  if (address)
    getdns_dict_destroy(address);

  return retvec;
}	
	
#endif // HAVE_GETDNS
