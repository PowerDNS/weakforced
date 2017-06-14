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
#include "misc.hh"
#include <mutex>
#include <thread>
#include <tuple>
#include "json11.hpp"
#include "login_tuple.hh"

typedef std::tuple<bool, KeyValVector> CustomFuncReturn;

struct CustomFuncArgs {
  std::map<std::string, std::string> attrs; // additional attributes
  std::map<std::string, std::vector<std::string>> attrs_mv; // additional multi-valued attributes

  void setAttrs(const json11::Json& msg) {
    LoginTuple lt;
    lt.setLtAttrs(msg);
    attrs = std::move(lt.attrs);
    attrs_mv = std::move(lt.attrs_mv);
  }

  Json to_json() const
  {
    using namespace json11;
    Json::object jattrs;

    for (auto& i : attrs_mv) {
      jattrs.insert(make_pair(i.first, Json(i.second)));
    }
    for (auto& i : attrs) {
      jattrs.insert(make_pair(i.first, Json(i.second)));
    }
    return Json::object{
      {"attrs", jattrs},
	};
  }

  std::string serialize() const
  {
    Json msg=to_json();
    return msg.dump();
  }
};

typedef std::function<CustomFuncReturn(const CustomFuncArgs&)> custom_func_t;
