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

#include "replfwd.hh"
#include "wforce-sibling.hh"
#include "sstuff.hh"
#include "dolog.hh"
#include <chrono>
#include <thread>
#include <sys/resource.h>
#include "base64.hh"
#include "boost/date_time/gregorian/gregorian.hpp"

using std::thread;

void parsePingCmd(const drogon::HttpRequestPtr& req, const std::string& command, const drogon::HttpResponsePtr& resp)
{
  resp->setStatusCode(drogon::k200OK);
  resp->setBody(R"({"status":"ok"})");
}

void parseAddSibling(const drogon::HttpRequestPtr& req, const drogon::HttpResponsePtr& resp, const std::string& command,
                     GlobalStateHolder<vector<shared_ptr<Sibling>>>& siblings)
{
  json11::Json msg;
  string err;

  resp->setStatusCode(drogon::k200OK);

  msg = json11::Json::parse(req->bodyData(), err);
  if (msg.is_null()) {
    resp->setStatusCode(drogon::k400BadRequest);
    json11::Json::object jobj {{"status", "failure"}, {"reason", err}};
    resp->setBody(json11::Json(jobj).string_value());
  }
  else {
    try {
      std::string sibling_host;
      int sibling_port;
      bool send_sdb = true;
      bool send_wlbl = true;

      if (command != "addReplicationForwarderSrc") {
        if (msg["sibling_host"].is_null() ||
            msg["sibling_port"].is_null()) {
          throw WforceException("One of mandatory parameters [sibling_host, sibling_port] is missing");
        }
        sibling_port = msg["sibling_port"].int_value();
      }
      else {
        if (msg["sibling_host"].is_null()) {
          throw WforceException("Mandatory parameters [sibling_host] is missing");
        }
        sibling_port = Sibling::defaultPort;
      }
      sibling_host = msg["sibling_host"].string_value();
      bool has_encryption_key = false;
      std::string encryption_key;
      if (!msg["encryption_key"].is_null()) {
        has_encryption_key = true;
        encryption_key = msg["encryption_key"].string_value();
      }

      if (!msg["send_sdb"].is_null()) {
        send_sdb = msg["send_sdb"].bool_value();
      }
      if (!msg["send_wlbl"].is_null()) {
        send_wlbl = msg["send_wlbl"].bool_value();
      }

      Sibling::Protocol proto = Sibling::Protocol::UDP;
      if (command != "addReplicationForwarderSrc") {
        if (!msg["sibling_protocol"].is_null()) {
          proto = Sibling::stringToProtocol(msg["sibling_protocol"].string_value());
        }
        if (proto == Sibling::Protocol::NONE) {
          throw WforceException(std::string("Invalid sibling_protocol: ") + msg["sibling_protocol"].string_value());
        }
      }
      else {
        proto = Sibling::Protocol::NONE;
      }
      std::string error_msg;
      if (!has_encryption_key) {
        if (!addSibling(sibling_host, sibling_port, proto, siblings, error_msg, send_sdb, send_wlbl)) {
          throw WforceException(error_msg);
        }
      }
      else {
        if (!addSiblingWithKey(sibling_host, sibling_port, proto, siblings, error_msg, encryption_key, send_sdb, send_wlbl)) {
          throw WforceException(error_msg);
        }
      }
    }
    catch (const WforceException& e) {
      resp->setStatusCode(drogon::k500InternalServerError);
      json11::Json::object jobj {{"status", "failure"}, {"reason", e.reason}};
      resp->setBody(json11::Json(jobj).string_value());
      errlog("Exception in command [%s] exception: %s", command, e.reason);
    }
  }
  if (resp->getStatusCode() == drogon::k200OK)
    resp->setBody(R"({"status":"ok"})");
}

void parseAddSiblingCmd(const drogon::HttpRequestPtr& req, const std::string& command, const drogon::HttpResponsePtr& resp)
{
  parseAddSibling(req, resp, command, g_replication.getSiblings());
  incPrometheusCommandMetric("addSibling");
}

void parseAddReplFwdDestCmd(const drogon::HttpRequestPtr& req, const std::string& command, const drogon::HttpResponsePtr& resp)
{
  parseAddSibling(req, resp, command, g_replication.getReplForwarders());
  incPrometheusCommandMetric("addReplicationForwarderDest");
}

void parseAddReplFwdSrcCmd(const drogon::HttpRequestPtr& req, const std::string& command, const drogon::HttpResponsePtr& resp)
{
  parseAddSibling(req, resp, command, g_replication.getReplForwardersSrc());
  incPrometheusCommandMetric("addReplicationForwarderSrc");
}

void parseRemoveSibling(const drogon::HttpRequestPtr& req, const drogon::HttpResponsePtr& resp, const std::string& command,
                        GlobalStateHolder<vector<shared_ptr<Sibling>>>& siblings)
{
  json11::Json msg;
  string err;

  resp->setStatusCode(drogon::k200OK);

  msg = json11::Json::parse(req->bodyData(), err);
  if (msg.is_null()) {
    resp->setStatusCode(drogon::k400BadRequest);
    json11::Json::object jobj {{"status", "failure"}, {"reason", err}};
    resp->setBody(json11::Json(jobj).string_value());
  }
  else {
    std::string sibling_host;
    int sibling_port;
    try {
      if (command != "removeReplicationForwarderSrc") {
        if (msg["sibling_host"].is_null() ||
            msg["sibling_port"].is_null()) {
          throw WforceException("One of mandatory parameters [sibling_host, sibling_port] is missing");
        }
        sibling_port = msg["sibling_port"].int_value();
      }
      else {
        if (msg["sibling_host"].is_null()) {
          throw WforceException("Mandatory parameters [sibling_host] is missing");
        }
        sibling_port = Sibling::defaultPort;
      }
      sibling_host = msg["sibling_host"].string_value();

      std::string error_msg;
      if (!removeSibling(sibling_host, sibling_port, siblings, error_msg)) {
        throw WforceException(error_msg);
      }
    }
    catch (const WforceException& e) {
      resp->setStatusCode(drogon::k500InternalServerError);
      json11::Json::object jobj {{"status", "failure"}, {"reason", e.reason}};
      resp->setBody(json11::Json(jobj).string_value());
      errlog("Exception in command [%s] exception: %s", command, e.reason);
    }
  }
  if (resp->getStatusCode() == drogon::k200OK)
    resp->setBody(R"({"status":"ok"})");
}

void parseRemoveSiblingCmd(const drogon::HttpRequestPtr& req, const std::string& command, const drogon::HttpResponsePtr& resp)
{
  parseRemoveSibling(req, resp, command, g_replication.getSiblings());
  incPrometheusCommandMetric("removeSibling");
}

void parseRemoveReplFwdDestCmd(const drogon::HttpRequestPtr& req, const std::string& command, const drogon::HttpResponsePtr& resp)
{
  parseRemoveSibling(req, resp, command, g_replication.getReplForwarders());
  incPrometheusCommandMetric("removeReplicationForwarderDest");
}

void parseRemoveReplFwdSrcCmd(const drogon::HttpRequestPtr& req, const std::string& command, const drogon::HttpResponsePtr& resp)
{
  parseRemoveSibling(req, resp, command, g_replication.getReplForwardersSrc());
  incPrometheusCommandMetric("removeReplicationForwarderSrc");
}

static inline std::string boolToString(bool b)
{
  return b ? "true" : "false";
}

void parseSetSiblings(const drogon::HttpRequestPtr& req, const drogon::HttpResponsePtr& resp, const std::string& command,
                      GlobalStateHolder<vector<shared_ptr<Sibling>>>& siblings)
{
  json11::Json msg;
  string err;
  std::vector<std::pair<int, std::vector<std::pair<int, std::string>>>> new_siblings;

  resp->setStatusCode(drogon::k200OK);

  msg = json11::Json::parse(req->bodyData(), err);
  if (msg.is_null()) {
    resp->setStatusCode(drogon::k400BadRequest);
    json11::Json::object jobj {{"status", "failure"}, {"reason", err}};
    resp->setBody(json11::Json(jobj).string_value());
  }
  else {
    try {
      if (msg["siblings"].is_null() || !msg["siblings"].is_array()) {
        throw WforceException("Mandatory parameter [siblings] is missing or is not an array");
      }
      auto siblings_array = msg["siblings"].array_items();
      for (auto& i : siblings_array) {
        int sibling_port = Sibling::defaultPort;
        if (command != "setReplicationForwarderSrcs") {
          if (i["sibling_host"].is_null() || i["sibling_port"].is_null()) {
            throw WforceException("Mandatory parameter(s) [sibling_host, sibling_port] are missing");
          }
          sibling_port = i["sibling_port"].int_value();
        }
        else {
          if (i["sibling_host"].is_null()) {
            throw WforceException("Mandatory parameter [sibling_host] is missing");
          }
        }
        std::string sibling_host = i["sibling_host"].string_value();

        bool send_sdb = true;
        bool send_wlbl = true;
        if (!i["send_sdb"].is_null()) {
          send_sdb = i["send_sdb"].bool_value();
        }
        if (!i["send_wlbl"].is_null()) {
          send_wlbl = i["send_wlbl"].bool_value();
        }

        std::string encryption_key;
        if (!i["encryption_key"].is_null()) {
          encryption_key = i["encryption_key"].string_value();
        }
        else {
          encryption_key = Base64Encode(g_replication.getEncryptionKey());
        }

        Sibling::Protocol proto = Sibling::Protocol::UDP;
        if (command != "setReplicationForwarderSrcs") {
          if (!i["sibling_protocol"].is_null()) {
            proto = Sibling::stringToProtocol(i["sibling_protocol"].string_value());
          }
          if (proto == Sibling::Protocol::NONE) {
            throw WforceException(std::string("Invalid sibling_protocol: ") + msg["sibling_protocol"].string_value());
          }
        }
        else {
          proto = Sibling::Protocol::NONE;
          sibling_port = Sibling::defaultPort;
        }
        new_siblings.emplace_back(std::pair<int, std::vector<std::pair<int, std::string>>>{0, {{0, createSiblingAddress(sibling_host, sibling_port, proto)},
                                                                                               {1, encryption_key},
                                                                                               {2, boolToString(send_sdb)},
                                                                                               {3, boolToString(send_wlbl)}}});
      }
      std::string error_msg;
      if (!setSiblingsWithKey(new_siblings, siblings, error_msg)) {
        throw WforceException(error_msg);
      }
    }
    catch (const WforceException& e) {
      resp->setStatusCode(drogon::k500InternalServerError);
      json11::Json::object jobj {{"status", "failure"}, {"reason", e.reason}};
      resp->setBody(json11::Json(jobj).string_value());
      errlog("Exception in command [%s] exception: %s", command, e.reason);
    }
  }
  if (resp->getStatusCode() == drogon::k200OK)
    resp->setBody(R"({"status":"ok"})");
}

void parseSetSiblingsCmd(const drogon::HttpRequestPtr& req, const std::string& command, const drogon::HttpResponsePtr& resp)
{
  parseSetSiblings(req, resp, command, g_replication.getSiblings());
  incPrometheusCommandMetric("setSiblings");
}

void parseSetReplFwdDestsCmd(const drogon::HttpRequestPtr& req, const std::string& command, const drogon::HttpResponsePtr& resp)
{
  parseSetSiblings(req, resp, command, g_replication.getReplForwarders());
  incPrometheusCommandMetric("setReplicationForwarderDests");
}

void parseSetReplFwdSrcsCmd(const drogon::HttpRequestPtr& req, const std::string& command, const drogon::HttpResponsePtr& resp)
{
  parseSetSiblings(req, resp, command, g_replication.getReplForwardersSrc());
  incPrometheusCommandMetric("setReplicationForwarderSrcs");
}

void parseGetSiblings(const drogon::HttpRequestPtr& req, const drogon::HttpResponsePtr& resp, const std::string& command,
                      GlobalStateHolder<vector<shared_ptr<Sibling>>>& siblings)
{
  auto local_siblings = siblings.getLocal();
  json11::Json::array jarray;
  for (auto i : *local_siblings) {
    json11::Json::object jsibling;
    jsibling.insert(make_pair("sibling_host", json11::Json(i->rem.toString())));
    jsibling.insert(make_pair("sibling_protocol", json11::Json(Sibling::protocolToString(i->proto))));
    if (i->proto != Sibling::Protocol::NONE) {
      jsibling.insert(make_pair("sibling_port", json11::Json(ntohs(i->rem.sin4.sin_port))));
      if (i->d_has_key)
        jsibling.insert(make_pair("encryption_key", json11::Json(Base64Encode(i->d_key))));
      jsibling.insert(make_pair("send_sdb", json11::Json(i->sdb_send)));
      jsibling.insert(make_pair("send_wlbl", json11::Json(i->wlbl_send)));
    }
    jarray.emplace_back(std::move(jsibling));
  }
  json11::Json ret_json = json11::Json::object{
      {"siblings", jarray}
  };
  resp->setStatusCode(drogon::k200OK);
  resp->setBody(ret_json.dump());
}

void parseGetSiblingsCmd(const drogon::HttpRequestPtr& req, const std::string& command, const drogon::HttpResponsePtr& resp)
{
  parseGetSiblings(req, resp, command, g_replication.getSiblings());
  incPrometheusCommandMetric("getSiblings");
}

void parseGetReplFwdDestsCmd(const drogon::HttpRequestPtr& req, const std::string& command, const drogon::HttpResponsePtr& resp)
{
  parseGetSiblings(req, resp, command, g_replication.getReplForwarders());
  incPrometheusCommandMetric("getReplicationForwarderDests");
}

void parseGetReplFwdSrcsCmd(const drogon::HttpRequestPtr& req, const std::string& command, const drogon::HttpResponsePtr& resp)
{
  parseGetSiblings(req, resp, command, g_replication.getReplForwardersSrc());
  incPrometheusCommandMetric("getReplicationForwarderSrcs");
}

void parseReadyzCmd(const drogon::HttpRequestPtr& req,
                    const std::string& command,
                    const drogon::HttpResponsePtr& resp)
{
  resp->setStatusCode(drogon::k200OK);
  incCommandStat("readyz");
  incPrometheusCommandMetric("readyz");
}

void registerWebserverCommands()
{
  g_webserver.registerFunc("ping", HTTPVerb::GET, WforceWSFunc(parsePingCmd));
  addPrometheusCommandMetric("ping");
  g_webserver.registerFuncNoAuth("readyz", HTTPVerb::GET, WforceWSFunc(parseReadyzCmd));
  addPrometheusCommandMetric("readyz");
  g_webserver.registerFunc("addSibling", HTTPVerb::POST, WforceWSFunc(parseAddSiblingCmd));
  addPrometheusCommandMetric("addSibling");
  g_webserver.registerFunc("addReplicationForwarderDest", HTTPVerb::POST, WforceWSFunc(parseAddReplFwdDestCmd));
  addPrometheusCommandMetric("addReplicationForwarderDest");
  g_webserver.registerFunc("addReplicationForwarderSrc", HTTPVerb::POST, WforceWSFunc(parseAddReplFwdSrcCmd));
  addPrometheusCommandMetric("addReplicationForwarderSrc");
  g_webserver.registerFunc("removeSibling", HTTPVerb::POST, WforceWSFunc(parseRemoveSiblingCmd));
  addPrometheusCommandMetric("removeSiblingCmd");
  g_webserver.registerFunc("removeReplicationForwarderDest", HTTPVerb::POST, WforceWSFunc(parseRemoveReplFwdDestCmd));
  addPrometheusCommandMetric("removeReplicationForwarderDest");
  g_webserver.registerFunc("removeReplicationForwarderSrc", HTTPVerb::POST, WforceWSFunc(parseRemoveReplFwdSrcCmd));
  addPrometheusCommandMetric("removeReplicationForwarderSrc");
  g_webserver.registerFunc("setSiblings", HTTPVerb::POST, WforceWSFunc(parseSetSiblingsCmd));
  addPrometheusCommandMetric("setSiblings");
  g_webserver.registerFunc("setReplicationForwarderDests", HTTPVerb::POST, WforceWSFunc(parseSetReplFwdDestsCmd));
  addPrometheusCommandMetric("setReplicationForwarderDests");
  g_webserver.registerFunc("setReplicationForwarderSrcs", HTTPVerb::POST, WforceWSFunc(parseSetReplFwdSrcsCmd));
  addPrometheusCommandMetric("setReplicationForwarderSrcs");
  g_webserver.registerFunc("getReplicationForwarderSrcs", HTTPVerb::GET, WforceWSFunc(parseGetReplFwdSrcsCmd));
  addPrometheusCommandMetric("setReplicationForwarderSrcs");
  g_webserver.registerFunc("getReplicationForwarderDests", HTTPVerb::GET, WforceWSFunc(parseGetReplFwdDestsCmd));
  addPrometheusCommandMetric("getReplicationForwarderDests");
  g_webserver.registerFunc("getSiblings", HTTPVerb::GET, WforceWSFunc(parseGetSiblingsCmd));
  addPrometheusCommandMetric("getSiblings");
}
