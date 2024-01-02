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
#include <string>
#include <mutex>
#include <vector>
#include "lock.hh"
#include "iputils.hh"
#include "wforce-geoip.hh"

#ifdef HAVE_MMDB

#include "maxminddb.h"

class WFGeoIP2DB
{
public:
  WFGeoIP2DB(const std::string& filename);
  WFGeoIP2DB();
  ~WFGeoIP2DB();
  WFGeoIP2DB(const WFGeoIP2DB&) = delete; // copy construct
  WFGeoIP2DB& operator=(const WFGeoIP2DB&) = delete; // copy assign
  WFGeoIP2DB(WFGeoIP2DB&&) = delete; // move construct
  WFGeoIP2DB& operator=(WFGeoIP2DB&&) = delete; // move assign
  
  std::string lookupCountry(const ComboAddress& address);
  std::string lookupISP(const ComboAddress& address);
  std::unique_ptr<WFGeoIPRecord> lookupCity(const ComboAddress& address);
  std::string lookupStringValue(const ComboAddress& address, const std::vector<std::pair<unsigned int, std::string>>& attrs);
  uint64_t lookupUIntValue(const ComboAddress& address, const std::vector<std::pair<unsigned int, std::string>>& attrs);
  bool lookupBoolValue(const ComboAddress& address, const std::vector<std::pair<unsigned int, std::string>>& attrs);
  double lookupDoubleValue(const ComboAddress& address, const std::vector<std::pair<unsigned int, std::string>>& attrs);

  static std::shared_ptr<WFGeoIP2DB> makeWFGeoIP2DB(const std::string& filename);
private:
  MMDB_s d_db;
  bool d_init = false;
  bool lookupDataValue(const ComboAddress& address, const std::vector<std::pair<unsigned int, std::string>>& attrs, MMDB_entry_data_s& ret_data);
  bool mmdbLookup(const std::string& ip, MMDB_lookup_result_s& res);
};

extern std::mutex geoip2_mutx;
extern std::map<std::string, std::shared_ptr<WFGeoIP2DB>> geoip2Map;

#endif // HAVE_MMDB
