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

#include "device_parser.hh"
#include <boost/regex.hpp>
#include <string>
#include <iostream>

// Example IMAP Client ID string (Apple Mail on MacOS):
// "name" "Mac OS X Mail" "version" "10.0 (3226)" "os" "Mac OS X" "os-version" "10.12 (16A323)" "vendor" "Apple Inc."
//
// This class maps IMAP Client ID fields as follows:
// os -> os.family
// os-version -> os.major, os.minor
// name -> imapc.family
// version -> imapc.major, imapc.minor
//
// IMAP fixes the field names that can be used so this doesn't
// need to be very extensible

boost::regex name_expr{"\"name\"\\s?\"(.*?)\""};
boost::regex os_expr{"\"os\"\\s?\"(.*?)\""};
boost::regex os_version_expr{"\"os-version\"\\s?\"(\\d+)\\.(\\d+).*?\""};
boost::regex version_expr{"\"version\"\\s?\"(\\d+)\\.(\\d+).*?\""};

IMAPClientID IMAPClientIDParser::parse(const std::string& clientid_str) const
{
  boost::smatch what;
  IMAPClientID ic;
  
  if (boost::regex_search(clientid_str, what, name_expr)) {
    ic.imapc.family = std::string(what[1].first, what[1].second);
  }
  if (boost::regex_search(clientid_str, what, os_expr)) {
    ic.os.family = std::string(what[1].first, what[1].second);
  }
  if (boost::regex_search(clientid_str, what, version_expr)) {
    ic.imapc.major = std::string(what[1].first, what[1].second);
    ic.imapc.minor = std::string(what[2].first, what[2].second);
  }
  if (boost::regex_search(clientid_str, what, os_version_expr)) {
    ic.os.major = std::string(what[1].first, what[1].second);
    ic.os.minor = std::string(what[2].first, what[2].second);
  }
  return ic;
}
