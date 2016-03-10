#include "wforce.hh"
#include "sstuff.hh"
#include "ext/json11/json11.hpp"
#include "ext/incbin/incbin.h"
#include "dolog.hh"
#include <thread>
#include <sstream>
#include <yahttp/yahttp.hpp>
#include "namespaces.hh"
#include <sys/time.h>
#include <sys/resource.h>
#include "ext/ctpl.h"
#include "base64.hh"

static int uptimeOfProcess()
{
  static time_t start=time(0);
  return time(0) - start;
}


bool compareAuthorization(YaHTTP::Request& req, const string &expected_password)
{
  // validate password
  YaHTTP::strstr_map_t::iterator header = req.headers.find("authorization");
  bool auth_ok = false;
  if (header != req.headers.end() && toLower(header->second).find("basic ") == 0) {
    string cookie = header->second.substr(6);

    string plain;
    B64Decode(cookie, plain);

    vector<string> cparts;
    stringtok(cparts, plain, ":");

    // this gets rid of terminating zeros
    auth_ok = (cparts.size()==2 && (0==strcmp(cparts[1].c_str(), expected_password.c_str())));
  }
  return auth_ok;
}

static void setLtAttrs(struct LoginTuple& lt, json11::Json& msg)
{
  using namespace json11;
  Json attrs = msg["attrs"];
  if (attrs.is_object()) {
    auto attrs_obj = attrs.object_items();
    for (auto it=attrs_obj.begin(); it!=attrs_obj.end(); ++it) {
      string attr_name = it->first;
      if (it->second.is_string()) {
	lt.attrs.insert(std::make_pair(attr_name, it->second.string_value()));
      }
      else if (it->second.is_array()) {
	auto av_list = it->second.array_items();
	std::vector<std::string> myvec;
	for (auto avit=av_list.begin(); avit!=av_list.end(); ++avit) {
	  myvec.push_back(avit->string_value());
	}
	lt.attrs_mv.insert(std::make_pair(attr_name, myvec));
      }
    }
  }

}

static void connectionThread(int id, int sock, ComboAddress remote, string password)
{
  Socket client(sock);
  using namespace json11;
  infolog("Webserver handling connection from %s", remote.toStringWithPort());
  string line;
  string request;
  YaHTTP::Request req;

  YaHTTP::AsyncRequestLoader yarl;
  yarl.initialize(&req);
  int timeout = 5;
  client.setNonBlocking();
  bool complete=false;
  try {
    while(!complete) {
      int bytes;
      char buf[1024];
      bytes = client.readWithTimeout(buf, sizeof(buf), timeout);
      if (bytes > 0) {
        string data(buf, bytes);
        complete = yarl.feed(data);
      } else {
        // read error OR EOF
        break;
      }
    }
    yarl.finalize();
  } catch (YaHTTP::ParseError &e) {
    // request stays incomplete
  } catch (NetworkError& e) {
    warnlog("Network error in web server: %s", e.what());
  }

  string command=req.getvars["command"];

  string callback;

  if(req.getvars.count("callback")) {
    callback=req.getvars["callback"];
    req.getvars.erase("callback");
  }

  req.getvars.erase("_"); // jQuery cache buster

  YaHTTP::Response resp;
  resp.headers["Content-Type"] = "application/json";
  resp.headers["Connection"] = "close";
  resp.version = req.version;
  resp.url = req.url;

  string ctype = req.headers["Content-Type"];
  if (!compareAuthorization(req, password)) {
    errlog("HTTP Request \"%s\" from %s: Web Authentication failed", req.url.path, remote.toStringWithPort());
    resp.status=401;
    std::stringstream ss;
    ss << "{\"status\":\"failure\", \"reason\":" << "Unauthorized" << "}";
    resp.body=ss.str();
    resp.headers["WWW-Authenticate"] = "basic realm=\"wforce\"";
  }
  else if ((command != "") && (ctype.compare("application/json") != 0)) {
    errlog("HTTP Request \"%s\" from %s: Content-Type not application/json", req.url.path, remote.toStringWithPort());
    resp.status = 415;
    std::stringstream ss;
    ss << "{\"status\":\"failure\", \"reason\":" << "\"Invalid Content-Type - must be application/json\"" << "}";
    resp.body=ss.str();
  }
  else if(command=="report" && req.method=="POST") {
    Json msg;
    string err;
    msg=Json::parse(req.body, err);
    if (msg.is_null()) {
      resp.status=500;
      std::stringstream ss;
      ss << "{\"status\":\"failure\", \"reason\":\"" << err << "\"}";
      resp.body=ss.str();
    }
    else {
      resp.postvars.clear();
      try {
	LoginTuple lt;
	lt.remote=ComboAddress(msg["remote"].string_value());
	lt.success=msg["success"].string_value() == "true"; // XXX this is wrong but works for dovecot
	lt.pwhash=msg["pwhash"].string_value();
	lt.login=msg["login"].string_value();
	setLtAttrs(lt, msg);
	lt.t=getDoubleTime();
	spreadReport(lt);
	g_stats.reports++;
	resp.status=200;
	{
	  g_luamultip->report(lt);
	}

	resp.body=R"({"status":"ok"})";
      }
      catch(...) {
	resp.status=500;
	resp.body=R"({"status":"failure"})";
      }
    }
  }
  else if(command=="allow" && req.method=="POST") {
    Json msg;
    string err;
    msg=Json::parse(req.body, err);
    if (msg.is_null()) {
      resp.status=500;
      std::stringstream ss;
      ss << "{\"status\":\"failure\", \"reason\":\"" << err << "\"}";
      resp.body=ss.str();
    }
    else {
      LoginTuple lt;
      lt.remote=ComboAddress(msg["remote"].string_value());
      lt.success=msg["success"].bool_value();
      lt.pwhash=msg["pwhash"].string_value();
      lt.login=msg["login"].string_value();
      setLtAttrs(lt, msg);
      int status=0;
      {
	status=g_luamultip->allow(lt);
      }
      g_stats.allows++;
      if(status < 0)
	g_stats.denieds++;
      msg=Json::object{{"status", status}};
      
      resp.status=200;
      resp.postvars.clear();
      resp.body=msg.dump();
    }
  }
  else if(command=="stats") {
    struct rusage ru;
    getrusage(RUSAGE_SELF, &ru);

    resp.status=200;
    Json my_json = Json::object {
      { "allows", (int)g_stats.allows },
      { "denieds", (int)g_stats.denieds },
      { "user-msec", (int)(ru.ru_utime.tv_sec*1000ULL + ru.ru_utime.tv_usec/1000) },
      { "sys-msec", (int)(ru.ru_stime.tv_sec*1000ULL + ru.ru_stime.tv_usec/1000) },
      { "uptime", uptimeOfProcess()},
      { "qa-latency", (int)g_stats.latency}
    };

    resp.headers["Content-Type"] = "application/json";
    resp.body=my_json.dump();
  }
  else {
    // cerr<<"404 for: "<<resp.url.path<<endl;
    resp.status=404;
  }

  if(!callback.empty()) {
    resp.body = callback + "(" + resp.body + ");";
  }

  std::ostringstream ofs;
  ofs << resp;
  string done;
  done=ofs.str();
  writen2(sock, done.c_str(), done.size());
}

unsigned int g_num_worker_threads = WFORCE_NUM_WORKER_THREADS;

void dnsdistWebserverThread(int sock, const ComboAddress& local, const std::string& password)
{
  infolog("Webserver launched on %s", local.toStringWithPort());
  auto localACL=g_ACL.getLocal();
  ctpl::thread_pool p(g_num_worker_threads);

  for(;;) {
    try {
      ComboAddress remote(local);
      int fd = SAccept(sock, remote);
      vinfolog("Got connection from %s", remote.toStringWithPort());
      if(!localACL->match(remote)) {
	close(fd);
	continue;
      }
      p.push(connectionThread, fd, remote, password);
    }
    catch(std::exception& e) {
      errlog("Had an error accepting new webserver connection: %s", e.what());
    }
  }
}
