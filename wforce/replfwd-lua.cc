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
#include "replfwd.hh"
#include "replfwd-replication.hh"
#include <thread>
#include <fstream>
#include "dolog.hh"
#include "base64.hh"
#include "replfwd-web.hh"
#include "wforce-prometheus.hh"
#include <boost/filesystem.hpp>

using std::thread;

ComboAddress g_sibling_listen_addr;
static vector<std::function<void(void)>>* g_launchWork;

vector<std::function<void(void)>> setupLua(LuaContext& c_lua,
                                           const std::string& config)
{
  g_launchWork = new vector<std::function<void(void)>>();

  c_lua.writeFunction("addACL", [](const std::string& domain) {
    g_webserver.addACL(domain);
  });

  c_lua.writeFunction("setACL", [](const vector<pair<int, string>>& parts) {
    NetmaskGroup nmg;
    for (const auto& p : parts) {
      nmg.addMask(p.second);
    }
    g_webserver.setACL(nmg);
  });

  c_lua.writeFunction("setKey", [](const std::string& key) -> bool {
    string newkey;
    if (B64Decode(key, newkey) < 0) {
      g_outputBuffer = string("Unable to decode ") + key + " as Base64";
      errlog("%s", g_outputBuffer);
      return false;
    }
    else {
      g_replication.setEncryptionKey(newkey);
      return true;
    }
  });

  c_lua.writeFunction("removeReplicationForwarderDest", [](const std::string& address) {
    removeSibling(address, g_replication.getReplForwarders(), g_outputBuffer);
  });

  c_lua.writeFunction("addReplicationForwarderDest", [](const std::string& address,
                                                        bool replicate_statsdb,
                                                        bool replicate_wlbl) {
    addSibling(address, g_replication.getReplForwarders(), g_outputBuffer, replicate_statsdb, replicate_wlbl);
  });

  c_lua.writeFunction("addReplicationForwarderDestWithKey", [](const std::string& address,
                                                               const std::string& key,
                                                               bool replicate_statsdb,
                                                               bool replicate_wlbl) {
    addSiblingWithKey(address, g_replication.getReplForwarders(), g_outputBuffer, key, replicate_statsdb, replicate_wlbl);
  });

  c_lua.writeFunction("setReplicationForwarderDestsWithKey", [](const std::vector<std::pair<int, std::vector<std::pair<int, std::string>>>>& parts) {
    setSiblingsWithKey(parts, g_replication.getReplForwarders(), g_outputBuffer);
  });

  c_lua.writeFunction("setReplicationForwarderSrcs", [](const std::vector<std::pair<int, std::string>>& parts) {
    std::vector<std::pair<int, std::string>> parts_proto_none;
    // Create a new array where the address has protocol none appended
    for (auto& i : parts) {
      if (i.second.find(":") != std::string::npos) {
        g_outputBuffer = string("Host/Address must not contain any : chars [") + i.second + "]";
        errlog("%s", g_outputBuffer);
        return;
      }
      std::string new_addr = i.second + ":" + Sibling::protocolToString(Sibling::Protocol::NONE);
      parts_proto_none.emplace_back(make_pair(i.first, new_addr));
    }
    setSiblings(parts_proto_none, g_replication.getReplForwardersSrc(), g_outputBuffer);
  });

  c_lua.writeFunction("addReplicationForwarderSrc", [](const std::string& address) {
    ComboAddress ca;
    Sibling::Protocol proto;
    try {
      parseSiblingString(address, ca, proto);
    }
    catch (const WforceException& e){
      errlog(e.reason.c_str());
      g_outputBuffer += e.reason;
      return;
    }
    addSibling(std::make_shared<Sibling>(ca, Sibling::Protocol::NONE), g_replication.getReplForwardersSrc(), g_outputBuffer);
  });

  c_lua.writeFunction("removeReplicationForwarderSrc", [](const std::string& address) {
    removeSibling(address, g_replication.getReplForwardersSrc(), g_outputBuffer);
  });

  c_lua.writeFunction("addSibling", [](const std::string& address) {
    addSibling(address, g_replication.getSiblings(), g_outputBuffer);
  });

  c_lua.writeFunction("addSiblingWithKey", [](const std::string& address, const std::string& key) {
    addSiblingWithKey(address, g_replication.getSiblings(), g_outputBuffer, key);
  });

  c_lua.writeFunction("setSiblings", [](const std::vector<std::pair<int, std::string>>& parts) {
    setSiblings(parts, g_replication.getSiblings(), g_outputBuffer);
  });

  c_lua.writeFunction("setSiblingsWithKey", [](const std::vector<std::pair<int, std::vector<std::pair<int, std::string>>>>& parts) {
    setSiblingsWithKey(parts, g_replication.getSiblings(), g_outputBuffer);
  });

  c_lua.writeFunction("setSiblingConnectTimeout", [](int timeout_ms) {
    setSiblingConnectTimeout(timeout_ms);
  });

  c_lua.writeFunction("setMaxSiblingQueueSize", [](unsigned int size) {
    g_replication.setMaxSiblingRecvQueueSize(size);
    setMaxSiblingSendQueueSize(static_cast<size_t>(size));
  });

  c_lua.writeFunction("siblingListener", [](const std::string& address) {
    ComboAddress ca;
    try {
      ca = ComboAddress(address, 4545);
    }
    catch (const WforceException& e) {
      const std::string errstr = (boost::format("%s [%s]. %s (%s)\n") % "siblingListener() error parsing address/port" %
                                  address % "Make sure to use IP addresses not hostnames" % e.reason).str();
      errlog(errstr.c_str());
      g_outputBuffer += errstr;
      return;
    }
    g_sibling_listen_addr = ca;
    auto launch = [ca]() {
      auto siblings = g_replication.getSiblings().getLocal();

      for (auto& s : *siblings) {
        s->checkIgnoreSelf(ca);
      }

      thread t1([ca]() { g_replication.receiveReplicationOperations(ca); });
      t1.detach();
      thread t2([ca]() { g_replication.receiveReplicationOperationsTCP(ca); });
      t2.detach();
    };
    if (g_launchWork)
      g_launchWork->push_back(launch);
    else
      launch();
  });

  c_lua.writeFunction("setMetricsNoPassword", []() {
    g_webserver.setMetricsNoPassword();
  });

  c_lua.writeFunction("webserver", [](const std::string& address, const std::string& password) {
    warnlog("Warning - webserver() configuration command is deprecated in favour of addListener() and will be removed in a future version");
    ComboAddress local;
    try {
      local = ComboAddress(address);
    }
    catch (const WforceException& e) {
      errlog("webserver() error parsing address/port [%s]. Make sure to use IP addresses not hostnames", address);
      return;
    }
    g_webserver.setBasicAuthPassword(password);
    g_webserver.addSimpleListener(local.toString(), local.getPort());
    auto launch = []() {
      thread t(WforceWebserver::start, &g_webserver);
      t.detach();
    };
    if (g_launchWork)
      g_launchWork->push_back(launch);
    else
      launch();
  });

  c_lua.writeFunction("addListener", [](const std::string& address, bool useSSL, const std::string cert_file,
                                                          const std::string& key_file, std::vector<std::pair<std::string, std::string>> opts) {
    ComboAddress local;
    try {
      local = ComboAddress(address);
    }
    catch (const WforceException& e) {
      errlog("addListener() error parsing address/port [%s]. Make sure to use IP addresses not hostnames", address);
      return;
    }
    if (useSSL) {
      // Check that the certificate and private key exist and are readable
      auto check_file = [](const std::string& filetype, const std::string& filename) -> bool {
        boost::filesystem::path p(filename);
        if (boost::filesystem::exists(p)) {
          if (!boost::filesystem::is_regular_file(p)) {
            errlog("addListener() %s file [%s] is not a regular file", filetype, filename);
            return false;
          }
        }
        else {
          errlog("addListener() %s file [%s] does not exist or cannot be read", filetype, filename);
          return false;
        }
        return true;
      };

      if (!(check_file("certificate", cert_file) && check_file("key", key_file))) {
        return;
      }
    }
    g_webserver.addListener(local.toString(), local.getPort(), useSSL, cert_file, key_file, false, opts);
    auto launch =[]() {
      thread t(WforceWebserver::start, &g_webserver);
      t.detach();
    };
    if (g_launchWork)
      g_launchWork->push_back(launch);
    else
      launch();
  });

  c_lua.writeFunction("setWebserverPassword", [](const std::string& password) {
    g_webserver.setBasicAuthPassword(password);
  });

  c_lua.writeFunction("setNumSiblingThreads", [](int numThreads) {
    // the number of threads used to process sibling reports
    g_replication.setNumSiblingThreads(numThreads);
  });

  std::ifstream ifs(config);
  if (!ifs)
    warnlog("Unable to read configuration from '%s'", config);
  else
    infolog("Read configuration from '%s'", config);

  c_lua.executeCode(ifs);
  auto ret = *g_launchWork;
  delete g_launchWork;
  g_launchWork = 0;
  return ret;
}
