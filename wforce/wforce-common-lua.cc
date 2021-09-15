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
#include "wforce.hh"
#include "wforce-common-lua.hh"
#include "wforce-web.hh"
#include <boost/filesystem.hpp>

using std::thread;

// These are Lua functions common to both wforce and trackalert (and crucially which have no differences in their
// implementation)
void setupWforceCommonLua(bool client, bool multi_lua, LuaContext& c_lua, std::vector<std::function<void(void)>>* launchwork)
{
  c_lua.writeFunction("shutdown", []() { _exit(0);} );

  if (!multi_lua) {
    c_lua.writeFunction("addListener", [client, launchwork](const std::string& address, bool useSSL, const std::string cert_file,
        const std::string& key_file, std::vector<std::pair<std::string, std::string>> opts) {
      if (client)
        return;
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
      if (launchwork)
        launchwork->push_back(launch);
      else
        launch();
    });
  }
  else {
    c_lua.writeFunction("addListener", [](const std::string& address, bool useSSL, const std::string cert_file,
        const std::string& private_key, std::vector<std::pair<std::string, std::string>> opts) { });
  }

  if (!multi_lua) {
    c_lua.writeFunction("controlSocket", [client, launchwork](const std::string& str) {
      ComboAddress local;
      try {
        local = ComboAddress(str, 4004);
      }
      catch (const WforceException& e) {
        errlog("controlSocket() error parsing address/port [%s]. Make sure to use IP addresses not hostnames", str);
        return;
      }
      if(client) {
        g_serverControl = local;
        return;
      }

      try {
        int sock = socket(local.sin4.sin_family, SOCK_STREAM, 0);
        if (sock < 0)
          throw std::runtime_error("Failed to create control socket");
        SSetsockopt(sock, SOL_SOCKET, SO_REUSEADDR, 1);
        SBind(sock, local);
        SListen(sock, 5);
        auto launch=[sock, local]() {
          thread t(controlThread, sock, local);
          t.detach();
        };
        if(launchwork)
          launchwork->push_back(launch);
        else
          launch();

      }
      catch(std::exception& e) {
        errlog("Unable to bind to control socket on %s: %s", local.toStringWithPort(), e.what());
        _exit(EXIT_FAILURE);
      }
    });
  }
  else {
    c_lua.writeFunction("controlSocket", [](const std::string& str) { });
  }

  if (!multi_lua) {
    c_lua.writeFunction("disableCurlPeerVerification", []() {
      g_curl_tls_options.verifyPeer = false;
      g_webhook_runner.disablePeerVerification();
    });
  }
  else {
    c_lua.writeFunction("disableCurlPeerVerification", []() { });
  }

  if (!multi_lua) {
    c_lua.writeFunction("disableCurlHostVerification", []() {
      g_curl_tls_options.verifyHost = false;
      g_webhook_runner.disableHostVerification();
    });
  }
  else {
    c_lua.writeFunction("disableCurlHostVerification", []() { });
  }

  if (!multi_lua) {
    c_lua.writeFunction("setCurlCABundleFile", [](const std::string& filename) {
      g_curl_tls_options.caCertBundleFile = filename;
      g_webhook_runner.setCACertBundleFile(filename);
    });
  }
  else {
    c_lua.writeFunction("setCurlCABundleFile", [](const std::string& filename) { });
  }

  if (!multi_lua) {
    c_lua.writeFunction("setCurlClientCertAndKey", [](const std::string& certfile, const std::string& keyfile) {
      g_curl_tls_options.clientCertFile = certfile;
      g_curl_tls_options.clientKeyFile = keyfile;
      g_webhook_runner.setClientCertAndKey(certfile, keyfile);
    });
  }
  else {
    c_lua.writeFunction("setCurlClientCertAndKey", [](const std::string& certfile, const std::string& keyfile) { });
  }
}
