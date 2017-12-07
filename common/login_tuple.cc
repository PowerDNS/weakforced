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

#include "login_tuple.hh"
#include "json11.hpp"

static DeviceCache dcache;

Json LoginTuple::to_json() const 
{
  using namespace json11;
  Json::object jattrs;
  Json::object jattrs_dev;

  for (auto& i : attrs_mv) {
    jattrs.insert(make_pair(i.first, Json(i.second)));
  }
  for (auto& i : attrs) {
    jattrs.insert(make_pair(i.first, Json(i.second)));
  }
  for (auto& i : device_attrs) {
    jattrs_dev.insert(make_pair(i.first, Json(i.second)));
  }

  return Json::object{
    {"login", login},
    {"success", success},
    {"t", (double)t}, 
    {"pwhash", pwhash},
    {"remote", remote.toString()},
    {"device_id", device_id},
    {"device_attrs", jattrs_dev},
    {"protocol", protocol},
    {"tls", tls},
    {"attrs", jattrs},
    {"policy_reject", policy_reject}};
}

std::string LoginTuple::serialize() const
{
  Json msg=to_json();
  return msg.dump();
}

void LoginTuple::from_json(const Json& msg, const std::shared_ptr<UserAgentParser> uap)
{
  login=msg["login"].string_value();
  pwhash=msg["pwhash"].string_value();
  t=msg["t"].number_value();
  success=msg["success"].bool_value();
  if (msg["remote"].is_string()) {
    remote=ComboAddress(msg["remote"].string_value());
    if (remote.isMappedIPv4()) {
      remote = remote.mapToIPv4();
    }
  }
  setLtAttrs(msg);
  setDeviceAttrs(msg, uap);
  device_id=msg["device_id"].string_value();
  protocol=msg["protocol"].string_value();
  tls=msg["tls"].bool_value();
  policy_reject=msg["policy_reject"].bool_value();
}

void LoginTuple::unserialize(const std::string& str) 
{
  string err;
  json11::Json msg=json11::Json::parse(str, err);
  from_json(msg);
}

void LoginTuple::setDeviceAttrs(const json11::Json& msg, const std::shared_ptr<UserAgentParser> uap)
{
  std::map<std::string, std::string> cached_attrs;
  json11::Json jattrs = msg["device_attrs"];
  if (jattrs.is_object()) {
    auto attrs_obj = jattrs.object_items();
    for (auto it=attrs_obj.begin(); it!=attrs_obj.end(); ++it) {
      string attr_name = it->first;
      if (it->second.is_string()) {
	device_attrs.insert(std::make_pair(attr_name, it->second.string_value()));
      }
    }
  }
  else if (dcache.readFromCache(msg["device_id"].string_value(), cached_attrs)) {
    device_attrs = std::move(cached_attrs);
  }
  else if (uap != nullptr) {  // client didn't supply, we will parse device_id ourselves
    std::string my_device_id=msg["device_id"].string_value();
    std::string my_protocol=msg["protocol"].string_value();

    // parse using the uap-cpp Parser
    if ((my_protocol.compare("http") == 0) ||
        (my_protocol.compare("https") == 0)) {
      UserAgent ua = uap->parse(my_device_id);
      device_attrs.insert(std::make_pair("device.family", ua.device.family));
      device_attrs.insert(std::make_pair("device.model", ua.device.model));
      device_attrs.insert(std::make_pair("device.brand", ua.device.brand));
      device_attrs.insert(std::make_pair("os.family", ua.os.family));
      device_attrs.insert(std::make_pair("os.major", ua.os.major));
      device_attrs.insert(std::make_pair("os.minor", ua.os.minor));
      device_attrs.insert(std::make_pair("browser.family", ua.browser.family));
      device_attrs.insert(std::make_pair("browser.major", ua.browser.major));
      device_attrs.insert(std::make_pair("browser.minor", ua.browser.minor));
    }
    else if ((my_protocol.compare("imap") == 0) ||
             (my_protocol.compare("imaps") == 0)) {
      IMAPClientIDParser imap_parser;
      IMAPClientID ic = imap_parser.parse(my_device_id);
      device_attrs.insert(std::make_pair("imapc.family", ic.imapc.family));
      device_attrs.insert(std::make_pair("imapc.major", ic.imapc.major));
      device_attrs.insert(std::make_pair("imapc.minor", ic.imapc.minor));
      device_attrs.insert(std::make_pair("os.family", ic.os.family));
      device_attrs.insert(std::make_pair("os.major", ic.os.major));
      device_attrs.insert(std::make_pair("os.minor", ic.os.minor));
    }
    else if (my_protocol.compare("mobileapi") == 0) {
      OXMobileAppDeviceParser oxmad_parser;
      OXMobileAppDevice oxmad = oxmad_parser.parse(my_device_id);
      device_attrs.insert(std::make_pair("os.family", oxmad.os.family));
      device_attrs.insert(std::make_pair("os.major", oxmad.os.major));
      device_attrs.insert(std::make_pair("os.minor", oxmad.os.minor));
      device_attrs.insert(std::make_pair("app.name", oxmad.app.name));
      device_attrs.insert(std::make_pair("app.brand", oxmad.app.brand));
      device_attrs.insert(std::make_pair("app.major", oxmad.app.major));
      device_attrs.insert(std::make_pair("app.minor", oxmad.app.minor));
      device_attrs.insert(std::make_pair("device.family", oxmad.device.family));
    }
    dcache.addToCache(my_device_id, device_attrs);
  }
}

void LoginTuple::setLtAttrs(const json11::Json& msg)
{
  json11::Json jattrs = msg["attrs"];
  if (jattrs.is_object()) {
    auto attrs_obj = jattrs.object_items();
    for (auto it=attrs_obj.begin(); it!=attrs_obj.end(); ++it) {
      string attr_name = it->first;
      if (it->second.is_string()) {
	attrs.insert(std::make_pair(attr_name, it->second.string_value()));
      }
      else if (it->second.is_array()) {
	auto av_list = it->second.array_items();
	std::vector<std::string> myvec;
	for (auto avit=av_list.begin(); avit!=av_list.end(); ++avit) {
	  myvec.push_back(avit->string_value());
	}
	attrs_mv.insert(std::make_pair(attr_name, myvec));
      }
    }
  }
}

std::string LtAttrsToString(const LoginTuple& lt)
{
  std::ostringstream os;
  os << "attrs={";
  for (auto i= lt.attrs.begin(); i!=lt.attrs.end(); ++i) {
    os << i->first << "="<< "\"" << i->second << "\"";
    if (i != --(lt.attrs.end()))
      os << ", ";
  }
  for (auto i = lt.attrs_mv.begin(); i!=lt.attrs_mv.end(); ++i) {
    if (i == lt.attrs_mv.begin())
      os << ", ";
    os << i->first << "=[";
    std::vector<std::string> vec = i->second;
    for (auto j = vec.begin(); j!=vec.end(); ++j) {
      os << "\"" << *j << "\"";
      if (j != --(vec.end()))
	os << ", ";
    }
    os << "]";
    if (i != --(lt.attrs_mv.end()))
      os << ", ";
  }
  os << "} ";
  return os.str();
}

std::string DeviceAttrsToString(const LoginTuple& lt)
{
  std::ostringstream os;
  os << "device_attrs={";
  for (auto i= lt.device_attrs.begin(); i!=lt.device_attrs.end(); ++i) {
    os << i->first << "="<< "\"" << i->second << "\"";
    if (i != --(lt.device_attrs.end()))
      os << ", ";
  }
  os << "} ";
  return os.str();
}
