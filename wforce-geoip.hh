#pragma once
#include <string>
#include <GeoIP.h>
#include "wforce.hh"

class WFGeoIPDB
{
public:
  WFGeoIPDB()
  {
    gi_v4 = gi_v6 = NULL;
  }

  ~WFGeoIPDB()
  {
    if (gi_v4) {
      GeoIP_delete(gi_v4);
    }
    if (gi_v6) {
      GeoIP_delete(gi_v6);
    }
  }

  // Only load it if someone wants to use GeoIP, otherwise it's a waste of RAM
  void initGeoIPDB();
  // This will lookup in either the v4 or v6 GeoIP DB, depending on what address is
  std::string const lookupCountry(const ComboAddress& address);

private:
  // GeoIPDB seems to have different DBs for v4 and v6 addresses, hence two DBs
  GeoIP *gi_v4;
  GeoIP *gi_v6;
};

extern WFGeoIPDB g_wfgeodb;
