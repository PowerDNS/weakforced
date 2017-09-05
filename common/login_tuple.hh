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
#include "misc.hh"
#include "iputils.hh"
#include "json11.hpp"
#include "device_parser.hh"

using namespace json11;

struct LoginTuple
{
  double t;
  ComboAddress remote;
  string login;
  string pwhash;
  string device_id;
  std::map<std::string, std::string> device_attrs;
  string protocol;
  bool tls;
  bool success;
  std::map<std::string, std::string> attrs; // additional attributes
  std::map<std::string, std::vector<std::string>> attrs_mv; // additional multi-valued attributes
  bool policy_reject;
  Json to_json() const;
  std::string serialize() const;
  void from_json(const Json& msg, const std::shared_ptr<UserAgentParser> uap=std::shared_ptr<UserAgentParser>());
  void unserialize(const std::string& src);
  void setLtAttrs(const json11::Json& msg);

  bool operator<(const LoginTuple& r) const
  {
    if(std::tie(t, login, pwhash, success) < std::tie(r.t, r.login, r.pwhash, r.success))
      return true;
    ComboAddress cal(remote);
    ComboAddress car(r.remote);
    cal.sin4.sin_port=0;
    car.sin4.sin_port=0;
    return cal < car;
  }
private:
  void setDeviceAttrs(const json11::Json& msg, const std::shared_ptr<UserAgentParser> uap);
};

std::string LtAttrsToString(const LoginTuple& lt);
std::string DeviceAttrsToString(const LoginTuple& lt);
