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

#include "config.h"
#ifdef HAVE_GEOIP
#include "wforce-geoip.hh"
#include "wforce_exception.hh"
#include "dolog.hh"

WFGeoIPDB g_wfgeodb;

void WFGeoIPDB::initGeoIPDB(WFGeoIPDBType db_types)
{
  if (db_types & WFGeoIPDBType::GEOIP_COUNTRY) {
    if (!gi_v4)
      gi_v4 = openGeoIPDB(GEOIP_COUNTRY_EDITION, "v4 country");
  }
  if (db_types & WFGeoIPDBType::GEOIP_COUNTRY_V6) {
    if (!gi_v6)
      gi_v6 = openGeoIPDB(GEOIP_COUNTRY_EDITION_V6, "v6 country");
  }
  if (db_types & WFGeoIPDBType::GEOIP_CITY) {
    if (!gi_city_v4)
      gi_city_v4 = openGeoIPDB(GEOIP_CITY_EDITION_REV1, "v4 city");
  }
  if (db_types & WFGeoIPDBType::GEOIP_CITY_V6) {
    if (!gi_city_v6)
      gi_city_v6 = openGeoIPDB(GEOIP_CITY_EDITION_REV1_V6, "v6 city");
  }
  if (db_types & WFGeoIPDBType::GEOIP_ISP) {
    if (!gi_isp_v4)
      gi_isp_v4 = openGeoIPDB(GEOIP_ISP_EDITION, "v4 isp");
  }
  if (db_types & WFGeoIPDBType::GEOIP_ISP_V6) {
    if (!gi_isp_v6)
      gi_isp_v6 = openGeoIPDB(GEOIP_ISP_EDITION_V6, "v6 isp");
  }
}

GeoIP* WFGeoIPDB::openGeoIPDB(GeoIPDBTypes db_type, const std::string& name)
{
  GeoIP* gip=NULL;
  if (GeoIP_db_avail(db_type)) {
      gip = GeoIP_open_type(db_type, GEOIP_MEMORY_CACHE);
      if (!gip) {
	std::string myerr = "Unable to open geoip " + name + " db";
	errlog(myerr.c_str());
	throw WforceException(myerr);
      }
    }
    else {
      std::string myerr = "No geoip " + name + " db available";
      errlog(myerr.c_str());
      throw WforceException(myerr);
    }
  return gip;
}

void WFGeoIPDB::reload()
{
  WriteLock wl(&gi_rwlock);

  if (gi_v4) {
    GeoIP_delete(gi_v4);
    gi_v4 = NULL;
    gi_v4 = openGeoIPDB(GEOIP_COUNTRY_EDITION, "v4 country");
  }
  if (gi_v6) {
    GeoIP_delete(gi_v6);
    gi_v6 = NULL;
    gi_v6 = openGeoIPDB(GEOIP_COUNTRY_EDITION_V6, "v6 country");
  }
  if (gi_city_v4) {
    GeoIP_delete(gi_city_v4);
    gi_city_v4 = NULL;
    gi_city_v4 = openGeoIPDB(GEOIP_CITY_EDITION_REV1, "v4 city");
  }
  if (gi_city_v6) {
    GeoIP_delete(gi_city_v6);
    gi_city_v6 = NULL;
    gi_city_v6 = openGeoIPDB(GEOIP_CITY_EDITION_REV1_V6, "v6 city");
  }
  if (gi_isp_v4) {
    GeoIP_delete(gi_isp_v4);
    gi_isp_v4 = NULL;
    gi_isp_v4 = openGeoIPDB(GEOIP_ISP_EDITION, "v4 isp");
  }
  if (gi_isp_v6) {
    GeoIP_delete(gi_isp_v6);
    gi_isp_v6 = NULL;
    gi_isp_v6 = openGeoIPDB(GEOIP_ISP_EDITION_V6, "v4 isp");
  }
}

std::string WFGeoIPDB::lookupCountry(const ComboAddress& address) const
{
  GeoIPLookup gl;
  const char* retstr=NULL;
  std::string ret="";
  ReadLock rl(&gi_rwlock);

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

std::string WFGeoIPDB::lookupISP(const ComboAddress& address) const
{
  GeoIPLookup gl;
  const char* retstr=NULL;
  std::string ret="";
  ReadLock rl(&gi_rwlock);

  if (address.sin4.sin_family == AF_INET && gi_isp_v4 != NULL) {
    retstr = GeoIP_name_by_ipnum_gl(gi_isp_v4, ntohl(address.sin4.sin_addr.s_addr), &gl);
  }
  else if (gi_isp_v6 != NULL) { // v6 address
    retstr = GeoIP_name_by_ipnum_v6_gl(gi_isp_v6, address.sin6.sin6_addr, &gl);
  }
  if (retstr)
    ret = retstr;
  return ret;
}

WFGeoIPRecord WFGeoIPDB::lookupCity(const ComboAddress& address) const
{
  GeoIPRecord* gir=NULL;
  WFGeoIPRecord ret_wfgir = {};
  ReadLock rl(&gi_rwlock);

  if (address.sin4.sin_family == AF_INET && gi_city_v4 != NULL) {
    gir = GeoIP_record_by_ipnum(gi_city_v4, ntohl(address.sin4.sin_addr.s_addr));
  }
  else if (gi_city_v6 != NULL) { // v6 address
    gir = GeoIP_record_by_ipnum_v6(gi_city_v6, address.sin6.sin6_addr);
  }
  if (gir) {
    if (gir->country_code != NULL)
      ret_wfgir.country_code = gir->country_code;
    if (gir->country_name != NULL)
      ret_wfgir.country_name = gir->country_name;
    if (gir->region != NULL)
      ret_wfgir.region = gir->region;
    if (gir->city != NULL)
      ret_wfgir.city = gir->city;
    if (gir->postal_code != NULL)
      ret_wfgir.postal_code = gir->postal_code;
    if (gir->continent_code != NULL)
      ret_wfgir.continent_code = gir->continent_code;
    ret_wfgir.latitude = gir->latitude;
    ret_wfgir.longitude = gir->longitude;
  }
  return ret_wfgir;
}


#endif // HAVE_GEOIP
