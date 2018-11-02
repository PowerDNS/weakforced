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
#ifdef HAVE_MMDB
#include "wforce-geoip2.hh"
#include "wforce_exception.hh"
#include "dolog.hh"

std::mutex geoip2_mutx;
std::map<std::string, std::shared_ptr<WFGeoIP2DB>> geoip2Map;

WFGeoIP2DB::WFGeoIP2DB(const std::string& filename)
{
  int res;
  memset(&d_db, 0, sizeof(d_db));
  if ((res = MMDB_open(filename.c_str(), MMDB_MODE_MMAP, &d_db)) != MMDB_SUCCESS)
    throw WforceException(std::string("Cannot open ") + filename + std::string(": ") + std::string(MMDB_strerror(res)));
  d_init = true;
  debuglog("Opened MMDB database %s (type: %s version: %d.%d)", filename, d_db.metadata.database_type, d_db.metadata.binary_format_major_version, d_db.metadata.binary_format_minor_version);
}

WFGeoIP2DB::WFGeoIP2DB()
{
  memset(&d_db, 0, sizeof(d_db));
}

WFGeoIP2DB::~WFGeoIP2DB()
{
  MMDB_close(&d_db);
}

std::string WFGeoIP2DB::lookupCountry(const ComboAddress& address)
{
  MMDB_entry_data_s data;
  MMDB_lookup_result_s res;

  if (!mmdbLookup(address.toString(), res) ||
      (MMDB_get_value(&res.entry, &data, "country", "iso_code", NULL) != MMDB_SUCCESS) ||
      !data.has_data)
    return {};

  return std::string(data.utf8_string, data.data_size);
}

std::string WFGeoIP2DB::lookupISP(const ComboAddress& address)
{
  MMDB_entry_data_s data;
  MMDB_lookup_result_s res;

  if (!mmdbLookup(address.toString(), res) ||
      (MMDB_get_value(&res.entry, &data, "isp", NULL) != MMDB_SUCCESS) ||
      !data.has_data)
    return {};

  return std::string(data.utf8_string, data.data_size);
}

WFGeoIPRecord WFGeoIP2DB::lookupCity(const ComboAddress& address)
{
  WFGeoIPRecord ret_wfgir = {};
  MMDB_entry_data_s data;
  MMDB_lookup_result_s res;
  
  if (mmdbLookup(address.toString(), res)) {
    if ((MMDB_get_value(&res.entry, &data, "country", "iso_code", NULL)
         == MMDB_SUCCESS) && (data.has_data))
      ret_wfgir.country_code = std::string(data.utf8_string, data.data_size);
    if ((MMDB_get_value(&res.entry, &data, "country", "names", "en", NULL)
         == MMDB_SUCCESS) && (data.has_data))
      ret_wfgir.country_name = std::string(data.utf8_string, data.data_size);
    if ((MMDB_get_value(&res.entry, &data, "subdivisions", "0", "iso_code", NULL)
         == MMDB_SUCCESS) && (data.has_data))
      ret_wfgir.region = std::string(data.utf8_string, data.data_size);
    if ((MMDB_get_value(&res.entry, &data, "cities", "0", NULL)
         == MMDB_SUCCESS) && (data.has_data))
      ret_wfgir.city = std::string(data.utf8_string, data.data_size);
    else if ((MMDB_get_value(&res.entry, &data, "city", "names", "en", NULL)
              == MMDB_SUCCESS) && (data.has_data))
      ret_wfgir.city = std::string(data.utf8_string, data.data_size);
    if ((MMDB_get_value(&res.entry, &data, "continent", "code", NULL)
         == MMDB_SUCCESS) && (data.has_data))
      ret_wfgir.continent_code = std::string(data.utf8_string, data.data_size);
    if ((MMDB_get_value(&res.entry, &data, "postal", "code", NULL)
         == MMDB_SUCCESS) && (data.has_data))
      ret_wfgir.postal_code = std::string(data.utf8_string, data.data_size);
    if ((MMDB_get_value(&res.entry, &data, "location", "latitude", NULL)
         == MMDB_SUCCESS) && (data.has_data))
      ret_wfgir.latitude = data.double_value;
    if ((MMDB_get_value(&res.entry, &data, "location", "longitude", NULL)
         == MMDB_SUCCESS) && (data.has_data))
      ret_wfgir.longitude = data.double_value;    
  }
  return ret_wfgir;
}

bool WFGeoIP2DB::mmdbLookup(const std::string& ip, MMDB_lookup_result_s& res)
{
  int gai_err;
  int mmdb_err;

  if (!d_init)
    return false;
  
  res = MMDB_lookup_string(&d_db, ip.c_str(), &gai_err, &mmdb_err);

  if (gai_err != 0) {
    warnlog("MMDB_lookup_string(%s) failed: %s", ip, gai_strerror(gai_err));
  }
  else if (mmdb_err != MMDB_SUCCESS) {
    warnlog("MMDB_lookup_string(%s) failed: %s", ip, MMDB_strerror(mmdb_err));
  }
  else if (res.found_entry) {
    return true;
  }
  return false;
}

std::shared_ptr<WFGeoIP2DB> WFGeoIP2DB::makeWFGeoIP2DB(const std::string& filename)
{
  return std::make_shared<WFGeoIP2DB>(filename);
}

#endif // HAVE_MMDB
