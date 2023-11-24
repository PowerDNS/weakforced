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
#include "wforce-sibling.hh"
#include "wforce_exception.hh"
#include "wforce_ns.hh"
#include <stddef.h>
#include <netdb.h>

#define SYSLOG_NAMES

#include <syslog.h>
#include "sstuff.hh"
#include "misc.hh"
#include <netinet/tcp.h>
#include <limits>
#include <boost/lexical_cast.hpp>
#include "dolog.hh"
#include <unistd.h>
#include "sodcrypto.hh"
#include "iputils.hh"
#include "base64.hh"
#include "ext/threadname.hh"
#include "wforce-prometheus.hh"

using std::atomic;
using std::thread;

static std::atomic<int> sibling_connect_timeout(5000); // milliseconds
static std::atomic<size_t> sibling_queue_size(5000);

extern ComboAddress g_sibling_listen_addr;

Sibling::Sibling(const ComboAddress& ca) : Sibling(ca, Protocol::UDP)
{
}

Sibling::Sibling(const ComboAddress& ca,
                 const Protocol& p,
                 int timeout,
                 size_t queue_size,
                 bool sdb_send,
                 bool wlbl_send) : Sibling(ca, p, std::string(), timeout, queue_size, sdb_send, wlbl_send)
{
}

void Sibling::connectSibling()
{
  sockp = wforce::make_unique<Socket>(rem.sin4.sin_family, static_cast<int>(proto));
  if (proto == Protocol::UDP) {
    sockp->connect(rem);
  }
  else if (proto == Protocol::TCP) {
    if (!d_ignoreself) {
      try {
        sockp->setKeepAlive();
        sockp->connectWithTimeout(rem, connect_timeout);
      }
      catch (const NetworkError& e) {
        errlog("TCP Connect to Sibling %s failed (%s)", rem
            .toStringWithPort(), e.what());
        incPrometheusReplicationConnFail(rem.toStringWithPort());
      }
    }
  }
}

Sibling::Sibling(const ComboAddress& ca,
                 const Protocol& p,
                 const std::string& key,
                 int timeout,
                 size_t queue_size,
                 bool send_sdb,
                 bool send_wlbl) : rem(ca), proto(p),
                                      connect_timeout(timeout),
                                      max_queue_size(queue_size),
                                      queue_thread_run(true),
                                      sdb_send(send_sdb),
                                      wlbl_send(send_wlbl),
                                      d_ignoreself(false),
                                      d_key(key)
{
  if (!d_key.empty()) {
    d_has_key = true;
  }
  d_nonce.init();
  if (proto != Protocol::NONE) {
    {
      std::lock_guard<std::mutex> lock(mutx);
      connectSibling();
    }
    // std::thread is moveable
    queue_thread = std::thread([this]() {
      thread_local bool init = false;
      if (!init) {
        setThreadName("wf/sibling-worker");
        init = true;
      }
      while (true) {
        std::string msg;
        {
          std::unique_lock<std::mutex> lock(queue_mutx);
          while ((queue.size() == 0) && queue_thread_run) {
            queue_cv.wait(lock);
          }
          if (!queue_thread_run)
            return;
          msg = std::move(queue.front());
          queue.pop();
        }
        send(msg);
      }
    });
  }
}

Sibling::~Sibling()
{
  if (proto != Protocol::NONE) {
    {
      std::lock_guard<std::mutex> lock(queue_mutx);
      queue_thread_run = false;
    }
    queue_cv.notify_one();
    queue_thread.join();
  }
}

void Sibling::checkIgnoreSelf(const ComboAddress& ca)
{
  if (proto != Protocol::NONE) {
    ComboAddress actualLocal;
    actualLocal.sin4.sin_family = ca.sin4.sin_family;
    socklen_t socklen = actualLocal.getSocklen();

    if (getsockname(sockp->getHandle(), (struct sockaddr*) &actualLocal, &socklen) < 0) {
      return;
    }

    actualLocal.sin4.sin_port = ca.sin4.sin_port;
    if (actualLocal == rem) {
      d_ignoreself = true;
    }
  }
}

void Sibling::send(const std::string& msg)
{
  if (d_ignoreself)
    return;

  if (proto == Protocol::UDP) {
    if (::send(sockp->getHandle(), msg.c_str(), msg.length(), 0) <= 0) {
      ++failures;
      incPrometheusReplicationSent(rem.toStringWithPort(), false);
    }
    else {
      ++success;
      incPrometheusReplicationSent(rem.toStringWithPort(), true);
    }
  }
  else if (proto == Protocol::TCP) {
    // This needs protecting with a mutex because of the reconnect logic
    std::lock_guard<std::mutex> lock(mutx);
    // Try to send. In case of error, try to reconnect (but only once) and then try to send again
    for (int i = 0; i < 2; ++i) {
      uint16_t nsize = htons(msg.length());
      try {
        sockp->writen(std::string((char*) &nsize, sizeof(nsize)));
        sockp->writen(msg);
        ++success;
        incPrometheusReplicationSent(rem.toStringWithPort(), true);
        break;
      }
      catch (const NetworkError& e) {
        if (i == 0) {
          ++failures;
          incPrometheusReplicationSent(rem.toStringWithPort(), false);
          errlog("Error writing to Sibling %s, reconnecting (%s)", rem.toStringWithPort(), e.what());
          connectSibling();
        }
      }
    }
  }
}

void Sibling::queueMsg(const std::string& msg)
{
  {
    std::lock_guard<std::mutex> lock(queue_mutx);
    if (queue.size() >= max_queue_size) {
      errlog("Sibling::queueMsg: max sibling send queue size (%d) reached - dropping replication msg", max_queue_size);
      return;
    }
    else {
      queue.push(msg);
    }
  }
  queue_cv.notify_one();
}

// Utility functions for managing siblings

// siblingHostToAddress takes string representing either a hostname or an IP address
// and returns a string representing the IP address.
// Throws a WforceException if no valid IP address could be determined
std::string siblingHostToAddress(const std::string& host)
{
  std::string sibling_address;
  struct addrinfo* res=nullptr, *res0=nullptr;
  int error = getaddrinfo(host.c_str(), nullptr, nullptr, &res0);
  if (error) {
    if (res0)
      freeaddrinfo(res0);
    throw WforceException(std::string("Error calling getaddrinfo() for sibling host: ") + gai_strerror(error));
  }
  bool found_addr = false;
  for (res = res0; res != nullptr; res = res->ai_next) {
    sibling_address =
        ComboAddress(res->ai_addr, res->ai_addrlen).toString();
    found_addr = true;
    break;
  }
  if (res0)
    freeaddrinfo(res0);
  if (!found_addr) {
    throw WforceException(std::string("Could not determine IP address for sibling host: ") + host);
  }
  return sibling_address;
}

// createSiblingAddress returns a string that combines the host, port and protocol
// using ":" as the separator (this is the format used for Lua functions)
// For example "[::1]:8081:tcp"
// Both the ComboAddress constructor and Sibling::protocolToString can throw WforceException on error
std::string createSiblingAddress(const std::string& host, int port, Sibling::Protocol proto)
{
  std::string address;
  if (host.find(':') != string::npos) { // IPv6 needs special handling because it needs []
    address = ComboAddress(host, port).toStringWithPort() + ":" + Sibling::protocolToString(proto);
  }
  else {
    address = host + ":" + itoa(port) + ":" + Sibling::protocolToString(proto);
  }
  return address;
}

// parseSiblingString takes a string (as constructed by parseSiblingAddress() for example) representing
// a sibling IP address, port and protocol, and sets the ComboAddress and SiblingProtocol parameters
// based on the values in the string
// The ComboAddress constructor will throw a WforceException if the resulting ComboAddress is not valid
// If an invalid protocol is supplied, then UDP will be assumed
void parseSiblingString(const std::string& str, ComboAddress& ca, Sibling::Protocol& proto)
{
  std::vector<std::string> sres;
  boost::split(sres, str, boost::is_any_of(":"));
  std::string host_port;
  std::string address;

  // Get the Protocol
  try {
    proto = Sibling::stringToProtocol(sres.back());
    sres.pop_back();
    host_port = boost::join(sres, ":");
  }
  catch (const WforceException& e) {
    proto = Sibling::Protocol::UDP;
    host_port = str;
  }
  // Split the host and port if necessary
  // Possibilities are: v4 only (1.2.3.4), v6 only ([::1]), hostname only (host.example.com)
  // v4 plus port (1.2.3.4:4001), v6 plus port ([::1]:4002), hostname plus port (host.example.com:4004)
  boost::split(sres, host_port, boost::is_any_of(":"));
  if (sres.size() == 1) { // 1 only matches bare v4 or hostnames
    address = siblingHostToAddress(sres[0]);
  }
  else if (sres.size() == 2) { // 2 only v4 or hostname plus port
    address = siblingHostToAddress(sres[0]) + ":" + sres[1];
  }
  else { // Everything else we can leave as it is
    address = host_port;
  }
  ca = ComboAddress(address, Sibling::defaultPort);
}

// siblingAddressPortExists returns true if a sibling already exists in the supplied siblings vector
// with the same address and port as supplied in address, otherwise it returns false
bool siblingAddressPortExists(const GlobalStateHolder<vector<shared_ptr<Sibling>>>& siblings, const ComboAddress& address)
{
  auto local_siblings = siblings.getLocal();
  for (auto& s : *local_siblings) {
    if (s->rem == address) {
      return true;
    }
  }
  return false;
}

// removeSibling removes a sibling from the supplied vector.
// Only host and port are supplied, as protocol is irrelevant when removing
// If the removal was successful, returns true.
// If unsuccessful, returns false and output_buffer contains an error string
bool removeSibling(const std::string& host, int port,
                   GlobalStateHolder<vector<shared_ptr<Sibling>>>& siblings,
                   std::string& output_buffer)
{
  return removeSibling(createSiblingAddress(host, port, Sibling::Protocol::UDP), siblings, output_buffer);
}

// removeSibling removes a sibling from the supplied vector.
// Address parameter should be in the form created by createSiblingAddress()
// If the removal was successful, returns true.
// If unsuccessful, returns false and output_buffer contains an error string
bool removeSibling(const std::string& address,
                   GlobalStateHolder<vector<shared_ptr<Sibling>>>& siblings,
                   std::string& output_buffer)
{
  ComboAddress ca;
  Sibling::Protocol proto;
  bool retval = false;

  try {
    parseSiblingString(address, ca, proto);
  }
  catch (const WforceException& e) {
    const std::string errstr = (boost::format("%s [%s]. %s (%s)") % "removeSibling() error parsing address/port" %
                                address % "Make sure to use IP addresses not hostnames" % e.reason).str();
    errlog(errstr.c_str());
    output_buffer += errstr;
    return retval;
  }
  siblings.modify([ca, &retval](vector<shared_ptr<Sibling>>& v) {
    for (auto i = v.begin(); i != v.end();) {
      if ((i->get()->rem == ca)) {
        i = v.erase(i);
        retval = true;
        break;
      }
      else {
        i++;
      }
    }
  });

  // We don't remove any existing prometheus metrics for this sibling, otherwise we might cut off metrics before they are scraped
  // This means that of sibling membership is highly dynamic, the mwtrics for siblings will "grow"; this is considered to be acceptable.

  return retval;
}

// addSibling adds a sibling to the supplied vector.
// Address parameter should be in the form created by createSiblingAddress()
// If the add was successful, returns true.
// If unsuccessful, returns false and output_buffer contains an error string
bool addSibling(const std::string& address,
                GlobalStateHolder<vector<shared_ptr<Sibling>>>& siblings,
                std::string& output_buffer, bool send_sdb, bool send_wlbl)
{
  // Empty key string means no key for this sibling (i.e. global key will be used as previously)
  return addSiblingWithKey(address, siblings, output_buffer, std::string(), send_sdb, send_wlbl);
}

// addSibling adds a sibling to the supplied vector.
// If the add was successful, returns true.
// If unsuccessful, returns false and output_buffer contains an error string
bool addSibling(const std::string& host, int port, Sibling::Protocol proto,
                GlobalStateHolder<vector<shared_ptr<Sibling>>>& siblings,
                std::string& output_buffer, bool send_sdb, bool send_wlbl)
{
  return addSibling(createSiblingAddress(host, port, proto), siblings, output_buffer, send_sdb, send_wlbl);
}

// addSiblingWithKey adds a sibling to the supplied vector, with a supplied key for encrypting traffic to that sibling
// If the add was successful, returns true.
// If unsuccessful, returns false and output_buffer contains an error string
bool addSiblingWithKey(const std::string& host, int port, Sibling::Protocol proto,
                       GlobalStateHolder<vector<shared_ptr<Sibling>>>& siblings,
                       std::string& output_buffer,
                       const std::string& key, bool send_sdb, bool send_wlbl)
{
  return addSiblingWithKey(createSiblingAddress(host, port, proto), siblings, output_buffer, key, send_sdb, send_wlbl);
}

// addSiblingWithKey adds a sibling to the supplied vector, with a supplied key for encrypting traffic to that sibling
// Address parameter should be in the form created by createSiblingAddress()
// If the add was successful, returns true.
// If unsuccessful, returns false and output_buffer contains an error string
bool addSiblingWithKey(const std::string& address,
                       GlobalStateHolder<vector<shared_ptr<Sibling>>>& siblings,
                       std::string& output_buffer,
                       const std::string& key, bool send_sdb, bool send_wlbl)
{
  ComboAddress ca;
  Sibling::Protocol proto;

  string raw_key;
  if (B64Decode(key, raw_key) < 0) {
    output_buffer += string("Unable to decode ") + key + " as Base64";
    errlog("%s", output_buffer);
    return false;
  }

  try {
    parseSiblingString(address, ca, proto);
  }
  catch (const WforceException& e) {
    const std::string errstr = (boost::format("%s [%s]. %s (%s)") % "addSiblingWithKey() error parsing address/port" %
                                address % "Make sure to use IP addresses not hostnames" % e.reason).str();
    errlog(errstr.c_str());
    output_buffer += errstr;
    return false;
  }

  auto sibling = std::make_shared<Sibling>(ca, proto, raw_key, sibling_connect_timeout, sibling_queue_size, send_sdb, send_wlbl);
  sibling->checkIgnoreSelf(g_sibling_listen_addr);
  return addSibling(sibling, siblings, output_buffer);
}

// addSibling performs the actual modification of the siblings vector
bool addSibling(std::shared_ptr<Sibling> sibling, GlobalStateHolder<vector<shared_ptr<Sibling>>>& siblings, std::string& output_buffer)
{
  // Ensure the Sibling isn't already there
  if (siblingAddressPortExists(siblings, sibling->rem)) {
    const std::string errstr = (boost::format("%s [%s]") % "addSibling() cannot add duplicate sibling" %
                                sibling->rem.toStringWithPort()).str();
    errlog(errstr.c_str());
    output_buffer += errstr;
    return false;
  }
  // This is for sending when we know the port
  addPrometheusReplicationSibling(sibling->rem.toStringWithPort());
  // This is for receiving when the port may be ephemeral
  addPrometheusReplicationSibling(sibling->rem.toString());
  siblings.modify([sibling](vector<shared_ptr<Sibling>>& v) {
    v.push_back(sibling);
  });
  return true;
}

// setSiblings sets the siblings in the siblings vector using the arguments in the parts vector
// The parts vector is converted from the former simple list of index/address string pairs (from Lua, hence the index)
// to the new more complex list of lists which is designed to handle keys in addition to addresses
// If the add was successful, returns true.
// If unsuccessful, returns false and output_buffer contains an error string
bool setSiblings(const vector<std::pair<int, string>>& parts,
                 GlobalStateHolder<vector<shared_ptr<Sibling>>>& siblings,
                 std::string& output_buffer)
{
  std::vector<std::pair<int, std::vector<std::pair<int, std::string>>>> t_vec;
  for (auto& i : parts) {
    t_vec.emplace_back(
        // Note that an empty string indicates no key hence the std::string() here
        std::pair<int, std::vector<std::pair<int, std::string>>>{i.first, {{0, i.second}, {1, std::string()}}});
  }
  return setSiblingsWithKey(t_vec, siblings, output_buffer);
}

// Utility function to convert strings to boolean
bool toBool(const std::string& bool_str) {
  std::string str(bool_str);
  std::transform(str.begin(), str.end(), str.begin(), ::tolower);
  std::istringstream is(str);
  bool b;
  is >> std::boolalpha >> b;
  return b;
}

// setSiblings sets the siblings in the siblings vector using the arguments in the parts vector
// The parts vector is the new more complex list of lists which is designed to handle keys (and other parameters)
// in addition to addresses.
// The inner vector can contain 2 or 4 parameters (2 means address+key, 4 means address+key+send_sdb+send_wlbl)
// Any prometheus metrics associated with previously created siblings will not be removed
// If the add was successful, returns true.
// If unsuccessful, returns false and output_buffer contains an error string
bool setSiblingsWithKey(const std::vector<std::pair<int, std::vector<std::pair<int, std::string>>>>& parts,
                        GlobalStateHolder<vector<shared_ptr<Sibling>>>& siblings,
                        std::string& output_buffer)
{
  // Before we do anything, make sure we don't get any errors parsing the input
  for (const auto& p : parts) {
    try {
      ComboAddress ca;
      Sibling::Protocol proto;
      parseSiblingString(p.second[0].second, ca, proto);
      if ((p.second.size() != 2) && (p.second.size() != 4)) {
        errlog("setSiblings[WithKey](): Invalid values - was expecting 2 or 4 args, got %d", p.second.size());
        output_buffer += "Invalid number of args - was expecting 2 args (sibling address & key) or 4 (sibling address, key, replicate sdb, replicate wlbl)";
        return false;
      }
      string raw_key;
      if (B64Decode(p.second[1].second, raw_key) < 0) {
        output_buffer += string("Unable to decode ") + p.second[1].second + " as Base64";
        errlog("%s", output_buffer);
        return false;
      }
    }
    catch (const WforceException& e) {
      const std::string errstr = (boost::format("%s [%s]. %s (%s)") % "setSiblings() error parsing address/port" %
                                  p.second[0].second % "Reason: " % e.reason).str();
      errlog(errstr.c_str());
      output_buffer += errstr;
      return false;
    }
  }

  // Construct the new siblings
  vector<shared_ptr<Sibling>> v;
  for (const auto& p : parts) {
    bool send_sdb = true;
    bool send_wlbl = true;

    ComboAddress ca;
    Sibling::Protocol proto;

    string raw_key;
    B64Decode(p.second[1].second, raw_key);
    parseSiblingString(p.second[0].second, ca, proto);

    if (p.second.size() == 4) {
      send_sdb = toBool(p.second[2].second);
      send_wlbl = toBool(p.second[3].second);
    }

    bool skip = false;
    for (auto& s : v) {
      if (s->rem == ca) {
        errlog("setSiblings(): Two siblings with the same address and port are not allowed, ignoring the duplicate (%s)", p.second[0].second);
        skip = true;
        break;
      }
    }
    // Skip over duplicates
    if (skip)
      continue;

    // This is for sending when we know the port
    addPrometheusReplicationSibling(ca.toStringWithPort());
    // This is for receiving when the port may be ephemeral
    addPrometheusReplicationSibling(ca.toString());
    // Create the sibling after the Prometheus metrics so connfail stats get updated
    auto sibling = std::make_shared<Sibling>(ca, proto, raw_key, sibling_connect_timeout,
                                              sibling_queue_size, send_sdb, send_wlbl);
    sibling->checkIgnoreSelf(g_sibling_listen_addr);
    v.push_back(sibling);
  }
  siblings.setState(v);
  return true;
}

void setMaxSiblingSendQueueSize(size_t queue_size)
{
  sibling_queue_size.store(queue_size);
}

void setSiblingConnectTimeout(int timeout)
{
  sibling_connect_timeout.store(timeout);
}
