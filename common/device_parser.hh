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

#include "ext/uap-cpp/UaParser.h"
#include "lock.hh"
#include <unordered_map>

struct IMAPClient : Generic {
  std::string major;
  std::string minor;

  std::string toString() const { return family + " " + toVersionString(); }

  std::string toVersionString() const {
    return (major.empty() ? "0" : major) + "." + (minor.empty() ? "0" : minor);
  }
};

struct IMAPClientID {
  Agent os;
  IMAPClient imapc;
};

class IMAPClientIDParser {
public:
  IMAPClientID parse(const std::string&) const;
};

// The following parser is specific to Open-Xchange Mobile Apps
// Only invoked if "protocol" field is "mobileapi"
struct OXMobileApp {
  std::string major;
  std::string minor;
  std::string brand;
  std::string name;

  std::string toString() const { return brand + "/" + name + "/" + toVersionString(); }

  std::string toVersionString() const {
    return (major.empty() ? "0" : major) + "." + (minor.empty() ? "0" : minor);
  }
};

struct OXMobileAppDevice {
  Agent os;
  Device device;
  OXMobileApp app;
};

class OXMobileAppDeviceParser {
public:
  OXMobileAppDevice parse(const std::string&) const;
};

class DeviceCache {
public:
  bool readFromCache(const std::string&, std::map<std::string, std::string>&) const;
  void addToCache(const std::string&, const std::map<std::string, std::string>);
private:
  mutable pthread_rwlock_t d_rwlock = PTHREAD_RWLOCK_INITIALIZER;
  std::unordered_map<std::string, std::map<std::string, std::string>> d_devicemap;
};
