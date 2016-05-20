% WFORCE.CONF(5)
% Dovecot Oy
% 2016

# NAME
**wforce.conf** - configuration file for wforce daemon

# DESCRIPTION
This file is read by **wforce** and is a Lua script containing Lua
commands to implement (a) configuration of the wforce daemon and (b)
functions that control the operation of the wforce daemon in response
to HTTP commands, specifically "report", "allow" and "reset".

An alternative version of this file can be specified with

   wforce -C private_wforce.conf ...

The following functions are for configuration of wforce only, and
cannot be called inside the allow/report/reset functions:

* setACL(<list of netmasks>) - Set the access control list for the
  HTTP Server. For example, to allow access from any IP address, specify:

  setACL({"0.0.0.0/0"}) 

* addACL(<netmask>) - Add a netmask to the access control list for the
  HTTP server. For example, to allow access from 127.0.0.0/8, specify:

  addACL("127.0.0.0/8")

* setSiblings(<list of IP[:port]>) - Set the list of siblings to which
  all received reports should be forwarded. If port is not specified
  it defaults to 4001. For example:

  setSiblings({"127.0.1.2", "127.0.1.3:4004"})

* addSibling(<IP[:port]>) - Add a sibling to the list to which all
  received reports should be forwarded.  If port is not specified
  it defaults to 4001. For example:

  addSibling("192.168.1.23")

* siblingListener(<IP[:port]>) - Listen for reports from siblings on
  the specified IP address and port.  If port is not specified
  it defaults to 4001. For example:

  siblingListener("0.0.0.0:4001")

* webserver(<IP:port>, <password>) - Listen for HTTP commands on the
  specified IP address and port. The password is used to authenticate
  client connections using basic authentication. For example:

  webserver("0.0.0.0:8084", "super")

* controlSocket(<IP[:port]>) - Listen for control connections on the
  specified IP address and port. If port is not specified it defaults
  to 5199. For example:

  controlSocket("0.0.0.0:4004")

* setNumLuaStates(<num states>) - Set the number of Lua Contexts that
  will be created to run allow/report/reset commands. Defaults to 10
  if not specified. See also setNumWorkerThreads() and
  setNumSiblingThreads(). Should be at least equal to NumWorkerThreads
  + NumSiblingThreads. For example: 

  setNumLuaStates(10)

* setNumWorkerThreads(<num threads>) - Set the number of threads in
  the pool used to run allow/report/reset commands. Each thread uses a
  separate Lua Context, (see setNumLuaStates()). Defaults to 4 if not
  specified. For example: 

  setNumWorkerThreads(4)

* setNumSiblingThreads(<num threads>) - Set the number of threads in
  the pool used to process reports received from siblings. Each thread
  uses a separate Lua Context, the number of which is set with
  setNumLuaStates(). Defaults to 2 if not specified. For example:

  setNumSiblingThreads(2)

* initGeoIPDB() - Initializes the country-level IPv4 and IPv6 GeoIP
  databases. If either of these databases is not installed, this
  command will fail and wforce will not start. For example:

  initGeoIPDB()

* lookupCountry(<ComboAddress>) - Returns the two-character country
  code of the country that the IP address is located in. A
  ComboAddress object can be created with the newCA() function. For example:

  my_country = lookupCountry(my_ca)

* newDNSResolver(<resolver name>) - Create a new DNS resolver object with the
  specified name. Note this does not return the resolver object - that
  is achieved with the getDNSResolver() function. For example:

  newDNSResolver("MyResolver")

The following functions are available as part of the configuration or
within the allow/report/rest functions:

* newCA(<IP[:port]>) - Create an object representing an IP address (v4
  or v6) and optional port. For example:

  my_ca = newCA("192.128.12.82")

* getDNSResolver(<resolver name>) - Return a DNS resolver object
  corresponding to the name specified. For example:

  resolv = getDNSResolver("MyResolver")

* Resolver:addResolver(<ip address>, <port>) - Adds the specified IP address
  and port as a recursive server to use when resolving DNS queries. For
  example:

  resolv = getDNSResolver("MyResolver")
  resolv:addResolver("8.8.8.8", 53)

* Resolver:setRequestTimeout(<timeout>) - Sets the timeout in
  milliseconds. For example

  resolv = getDNSResolver("MyResolver")
  resolv:setRequestTimeout(100)

* Resolver:lookupAddrByName(<name>, [<num retries>]) - Performs DNS A record resolution
  for the specified name, returning an array of IP address strings. Optionally
  the number of retries can be specified. For example:

  resolv = getDNSResolver("MyResolver")
  resolv:lookupAddrByName("google.com")
  for i, addr in ipairs(dnames) do
      -- addr contains an IPv4 or IPv6 address
  end

* Resolver:lookupNameByAddr(<ComboAddress>, [num retries]) - Performs
  DNS PTR record resolution for the specified address, returning an
  array of names.  Optionally the number of retries can be
  specified. For example:

  resolv = getDNSResolver("MyResolver")
  resolv:lookupNameByAddr(lt.remote)
  for i, dname in ipairs(dnames) do
      -- dname is the resolved DNS name
  end

* Resolver:lookupRBL(<ComboAddress>, <rbl zone>, [num retries]) - Lookup an
  IP address in a RBL-formatted zone. Returns an array of IP address
  strings. Optionally the number of retries can be specified. For
  example:

  dnames = rblresolv:lookupRBL(lt.remote, "sbl.spamhaus.org")
  for i, dname in ipairs(dnames) do
    if (string.match(dname, "127.0.0.2"))
    then
	-- the RBL matched
    end
  end

... needs more work ...

# FILES
*/etc/wforce.conf*

# SEE ALSO
wforce(1)

