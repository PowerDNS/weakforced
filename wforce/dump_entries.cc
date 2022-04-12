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
#include <stddef.h>
#define SYSLOG_NAMES
#include <syslog.h>
#include <getopt.h>
#include <netinet/tcp.h>
#include <limits>
#include "minicurl.hh"
#include "wforce_exception.hh"
#include <iostream>
#include <fstream>
#include "iputils.hh"
#include "json11.hpp"
#include "base64.hh"
#include "sstuff.hh"
#include "dolog.hh"

using namespace std;
using namespace json11;

void print_help()
{
  cout<<"Syntax: dump_entries [-u,--uri wforce uri] [-l,--local-addr address] [-p,--local-port port] [-w,--wforce-pwd password] [-f,--filename file] [-h,--help]\n";
  cout<<"\n";
  cout<<"-u,--uri address          <Mandatory> Connect to wforce on the specified URI\n";
  cout<<"-l,--local-addr address   <Mandatory> Listen for results on the specified local IP address\n";
  cout<<"-p,--local-port port      <Mandatory> Listen on the specified port\n";
  cout<<"-w,--wforce-pwd password  <Mandatory> The password to authenticate to wforce\n";
  cout<<"-f,--filename file        <Optional>  Send the results to a file\n";
  cout<<"-h,--help                 Display this helpful message\n";
  cout<<"\n";
}

bool g_console = true;
bool g_docker = false;
LogLevel g_loglevel{LogLevel::Info};

int main(int argc, char** argv)
{
  string uri, local_addr, password, filename;
  int local_port=0;
  struct option longopts[]={
      {"uri", required_argument, 0, 'u'},
      {"local-addr", required_argument, 0, 'l'},
      {"local-port", required_argument, 0, 'p'},
      {"wforce-pwd", required_argument, 0, 'w'},
      {"filename", required_argument, 0, 'f'},
      {"help", 0, 0, 'h'},
      {0,0,0,0}
  };
  int longindex=0;
  for(;;) {
    int c=getopt_long(argc, argv, "hu:l:p:w:f:", longopts, &longindex);
    if(c==-1)
      break;
    switch(c) {
      case 'u':
        uri = optarg;
        break;
      case 'l':
        local_addr = optarg;
        break;
      case 'p':
        try {
          local_port = stoi(optarg);
        }
        catch (const std::exception& e) {
          cerr << "Could not parse local port - must be an integer" << endl;
          exit(EXIT_FAILURE);
        }
        break;
      case 'w':
        password = optarg;
        break;
      case 'f':
        filename = optarg;
        break;
      case 'h':
        print_help();
        exit(EXIT_SUCCESS);
        break;
    }
  }
  argc-=optind;
  argv+=optind;

  if ((uri.empty()) ||
      (local_addr.empty()) ||
      (local_port == 0) ||
      (password.empty())) {
    cout << "Missing mandatory arguments...\n";
    print_help();
    exit(EXIT_FAILURE);
  }

  // Open file if specified
  ofstream myfile;
  if (!filename.empty()) {
    myfile.open(filename, ios::out | ios::trunc);

    if (!myfile.is_open()) {
      cerr << "Could not open " << filename << " for writing." << endl;
      exit(EXIT_FAILURE);
    }
  }

  // Listen on local IP and port
  ComboAddress local_ca;
  try {
    local_ca = ComboAddress(local_addr, local_port);
  }
  catch (const WforceException& e) {
    cerr << "Error parsing local address and port " << local_addr << ", " << local_port << ". Make sure to use IP addresses not hostnames" << endl;
    exit(EXIT_FAILURE);
  }

  try {
    int listen_sock = socket(local_ca.sin4.sin_family, SOCK_STREAM, 0);
    if (listen_sock < 0)
      throw std::runtime_error("Failed to create socket");
    SSetsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, 1);
    SBind(listen_sock, local_ca);
    SListen(listen_sock, 1024);
    Socket listen_sock_wrapper(listen_sock); // This ensures the socket gets closed

    // Connect to the specified wforce address and port
    // and request to dump entries to our local address and port
    MiniCurl mc;
    MiniCurlHeaders mch;
    string error_msg;
    mch.insert(make_pair("Authorization", "Basic " + Base64Encode("wforce:"+password)));
    mch.insert(make_pair("Content-Type", "application/json"));
    Json::object post_json = {{"dump_host", local_addr},
                              {"dump_port", local_port}};
    if (uri.find("dumpEntries") == string::npos) {
      uri += "/?command=dumpEntries";
    }
    if (mc.postURL(uri, Json(post_json).dump(), mch, error_msg) != true) {
      cerr << "Post to wforce URI [" << uri << "] failed, error=" << error_msg;
      exit(EXIT_FAILURE);
    }

    // Finally wait for the dumped entries and print them out
    ComboAddress remote(local_ca);
    int fd = SAccept(listen_sock, remote);
    Socket sock(fd);
    string buf;

    try {
      for (;;) {
        sock.read(buf);
        if (buf.empty())
          break;
        if (filename.empty())
          cout << buf;
        else
          myfile << buf;
      }
    }
    catch (const NetworkError& e) {
      cerr << e.what() << endl;
    }
  }
  catch (const std::exception& e) {
    cerr << "dump_entries: error: " << e.what() << endl;
    exit(EXIT_FAILURE);
  }
  catch (const WforceException& e){
    cerr << "dump_entries: error: " << e.reason << endl;
    exit(EXIT_FAILURE);
  }
  if (filename.empty())
    cout << endl;
  else
    myfile << endl;
  if (myfile.is_open())
    myfile.close();
  exit(EXIT_SUCCESS);
}
