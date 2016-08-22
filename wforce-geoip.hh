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
