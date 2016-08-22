/*
 * This file is part of PowerDNS or weakforced.
 * Copyright -- PowerDNS.COM B.V. and its contributors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
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

#include "config.h"
#ifdef HAVE_GEOIP
#include "wforce-geoip.hh"
#include "dolog.hh"

WFGeoIPDB g_wfgeodb;

void WFGeoIPDB::initGeoIPDB()
  {
    if (gi_v4)
      return;
    if (GeoIP_db_avail(GEOIP_COUNTRY_EDITION)) {
      gi_v4 = GeoIP_open_type(GEOIP_COUNTRY_EDITION, GEOIP_MEMORY_CACHE);
      if (!gi_v4) {
	errlog("Unable to open geoip v4 country db");
	throw std::runtime_error("Unable to open geoip v4 country db");
      }
    }
    else {
      errlog("No geoip v4 country db available");
      throw std::runtime_error("No geoip v4 country db available");
    }
    if (gi_v6)
      return;
    if (GeoIP_db_avail(GEOIP_COUNTRY_EDITION_V6)) {
      gi_v6 = GeoIP_open_type(GEOIP_COUNTRY_EDITION_V6, GEOIP_MEMORY_CACHE);
      if (!gi_v6) {
	errlog("Unable to open geoip v6 country db");
	throw std::runtime_error("Unable to open geoip v6 country db");
      }
    }
    else {
      errlog("No geoip v6 country db available");
      throw std::runtime_error("No geoip v6 country db available");
    }
  }

std::string const WFGeoIPDB::lookupCountry(const ComboAddress& address)
  {
    GeoIPLookup gl;
    const char* retstr=NULL;
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

#endif // HAVE_GEOIP
