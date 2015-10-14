#include "wforce-geoip.hh"

WFGeoIPDB g_wfgeodb;

void WFGeoIPDB::initGeoIPDB()
  {
    if (gi_v4)
      GeoIP_delete(gi_v4);
    if (GeoIP_db_avail(GEOIP_COUNTRY_EDITION)) {
      gi_v4 = GeoIP_open_type(GEOIP_COUNTRY_EDITION, GEOIP_MEMORY_CACHE);
      if (!gi_v4)
	throw std::runtime_error("Unable to open geoip v4 country db");
    }
    else 
      throw std::runtime_error("No geoip v4 country db available");
    if (gi_v6)
      GeoIP_delete(gi_v6);
    if (GeoIP_db_avail(GEOIP_COUNTRY_EDITION_V6)) {
      gi_v6 = GeoIP_open_type(GEOIP_COUNTRY_EDITION_V6, GEOIP_MEMORY_CACHE);
      if (!gi_v6)
	throw std::runtime_error("Unable to open geoip v6 country db");
    }
    else 
      throw std::runtime_error("No geoip v6 country db available");
  }

std::string const WFGeoIPDB::lookupCountry(const ComboAddress& address)
  {
    GeoIPLookup gl;
    const char* retstr;
    std::string ret="";

    if (address.sin4.sin_family == AF_INET && gi_v4 != NULL) {
      retstr = GeoIP_country_code_by_ipnum_gl(gi_v4, ntohl(address.sin4.sin_addr.s_addr), &gl);
    }
    else if (gi_v6 != NULL) { // it's a v6 address (included mapped v4 of course)
      retstr = GeoIP_country_code_by_ipnum_v6_gl(gi_v6, address.sin6.sin6_addr, &gl);
    }
    if (retstr)
      ret = retstr;
    return ret;
  }
