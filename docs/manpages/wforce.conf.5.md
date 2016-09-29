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

# CONFIGURATION-ONLY FUNCTIONS

The following functions are for configuration of wforce only, and
cannot be called inside the allow/report/reset functions:

* setACL(\<list of netmasks\>) - Set the access control list for the
  HTTP Server. For example, to allow access from any IP address, specify:
  
		setACL({"0.0.0.0/0"}) 

* addACL(\<netmask\>) - Add a netmask to the access control list for the
  HTTP server. For example, to allow access from 127.0.0.0/8, specify:
  
		addACL("127.0.0.0/8")

* addWebHook(\<list of events\>, \<config key map\>) - Add a webhook
  for the specified events, with the specified configuration keys. See
  *wforce_webhook(5)* for the list of events, supported configuration
  keys, and details of the HTTP(S) POST sent to webhook URLs. For
  example:

		config_keys={}
		config_keys["url"] = "http://webhooks.example.com:8080/webhook/"
		config_keys["secret"] = "verysecretcode"
		events = { "report", "allow" }
		addWebHook(events, config_keys)
		
* setSiblings(\<list of IP[:port]\>) - Set the list of siblings to which
  stats db and blacklist data should be replicated. If port is not specified
  it defaults to 4001. For example:
  
		setSiblings({"127.0.1.2", "127.0.1.3:4004"})

* addSibling(\<IP[:port]\>) - Add a sibling to the list to which all
  stats db and blacklist data should be replicated.  If port is not specified
  it defaults to 4001. For example:
  
		addSibling("192.168.1.23")

* siblingListener(\<IP[:port]\>) - Listen for reports from siblings on
  the specified IP address and port.  If port is not specified
  it defaults to 4001. For example:
  
		siblingListener("0.0.0.0:4001")

* setReportSinks(\<list of IP[:port]\>) - Set the list of report sinks to which
  all received reports should be forwarded. Reports will be sent to
  the configured report sinks in a round-robin fashion if more than
  one is specified. If port is not specified it defaults to 4501. For
  example: 
  
		setReportSinks({"127.0.1.2", "127.0.1.3:4501"})

* addReportSink(\<IP[:port]\>) - Add a report sink to the list to which all
  received reports should be forwarded. Reports will be sent to
  the configured report sinks in a round-robin fashion if more than
  one is specified. If port is not specified it defaults to 4001. For
  example:
  
		addReportSink("192.168.1.23")

* webserver(\<IP:port\>, \<password\>) - Listen for HTTP commands on the
  specified IP address and port. The password is used to authenticate
  client connections using basic authentication. For example:
  
		webserver("0.0.0.0:8084", "super")

* controlSocket(\<IP[:port]\>) - Listen for control connections on the
  specified IP address and port. If port is not specified it defaults
  to 5199. For example: 
  
		controlSocket("0.0.0.0:4004")

* setKey(\<key\>) - Use the specified key for authenticating 
  connections from siblings. The key must be generated with makeKey()
  from the console. See *wforce(1)* for instructions on
  running a console client. For example:
  
		setKey("Ay9KXgU3g4ygK+qWT0Ut4gH8PPz02gbtPeXWPdjD0HE=")

* setNumLuaStates(\<num states\>) - Set the number of Lua Contexts that
  will be created to run allow/report/reset commands. Defaults to 10
  if not specified. See also setNumWorkerThreads() and
  setNumSiblingThreads(). Should be at least equal to NumWorkerThreads
  \+ NumSiblingThreads. For example: 
  
		setNumLuaStates(10)

* setNumWorkerThreads(\<num threads\>) - Set the number of threads in
  the pool used to run allow/report/reset commands. Each thread uses a
  separate Lua Context, (see setNumLuaStates()). Defaults to 4 if not
  specified. For example:
  
		setNumWorkerThreads(4)

* setNumSiblingThreads(\<num threads\>) - Set the number of threads in
  the pool used to process reports received from siblings. Each thread
  uses a separate Lua Context, the number of which is set with
  setNumLuaStates(). Defaults to 2 if not specified. For example:
  
		setNumSiblingThreads(2)

* setNumWebHookThreads(\<num threads\>) - Set the number of threads in
  the pool used to send webhook events. For example:

		setNumWebHookThreads(2)

* initGeoIPDB() - Initializes the country-level IPv4 and IPv6 GeoIP
  databases. If either of these databases is not installed, this
  command will fail and wforce will not start. For example:
  
		initGeoIPDB()

* newDNSResolver(\<resolver name\>) - Create a new DNS resolver object with the
  specified name. Note this does not return the resolver object - that
  is achieved with the getDNSResolver() function. For example:
  
		newDNSResolver("MyResolver")

* newStringStatsDB(\<stats db name\>, \<window size\>, \<num windows\>,
  \<field map\>) - Create a new StatsDB object with
  the specified name. Note this does not return the object - that is
  achieved with the getStringStatsDB() function. For example:
  
		field_map = {}
		field_map["countLogins"] = "int"
		field_map["diffPasswords"] = "hll"
		field_map["countCountries"] = "countmin"
		newStringStatsDB("OneHourDB", 900, 4, field_map)

* StringStatsDB:twSetv4Prefix(\<prefix\>) - Set the prefix to use for
  any IPv4 ComboAddress keys stored in the db. For example, specify 24 to
  group statistics for class C networks. For example:
  
		statsdb:twSetv4Prefix(24)

* StringStatsDB:twSetv6Prefix(\<prefix\>) - Set the prefix to use for
  any IPv6 ComboAddress keys stored in the db. For example:
  
		statsdb:twSetv4Prefix(64)

* StringStatsDB:twSetMaxSize(\<size\>) - Set the maximum number of keys
  to be stored in the db. When the db reaches that size, keys will be
  expired on a LRU basis. The default size is 500K keys. For example:
  
		statsdb:twSetMaxSize(1000000)

* StringStatsDB:twEnableReplication() - Enable replication for this
  db; only makes a difference if siblings have been configured. For
  example:
  
		statsdb:twEnableReplication()

* blacklistPersistDB(\<ip\>, \<port\>) - Make the blacklist persistent
  by storing entries in the specified redis DB. The IP address is a
  string, and port should be 6379 unless you are running redis
  on a non-standard port. If this option is specified, wforce will
  read all the blacklist entries from the redis DB on startup. For example:
  
		blacklistPersistDB("127.0.0.1", 6379)

* blacklistPersistReplicated() - Store blacklist entries that have
  been replicated in the redis DB. By default, replicated blacklist 
  entries will not be persisted. If you use a local
  redis DB for each wforce server, then use this config option. If
  you use a single redis instance (or cluster) for all wforce servers,
  then you should not specify this option, as it will cause
  unnecessary writes to the redis DB. For example:
  
		blacklistPersistReplicated()

* setAllow(\<allow func\>) - Tell wforce to use the specified Lua
  function for handling all "allow" commands. For example:
  
		setAllow(allow)

* setReport(\<report func\>) - Tell wforce to use the specified Lua
  function for handling all "report" commands. For example:
  
		setReport(report)

* setReset(\<reset func\>) - Tell wforce to use the specified Lua
  function for handling all "reset" commands. For example:
  
		setReset(reset)

* setCanonicalize(\<canonicalize func\>) - Tell wforce to use the
  specified Lua function for handling all "canonicalisation"
  functionality. This function is responsible for mapping multiple
  aliases for a user to a single login string, e.g. ncook, neil.cook
  and neil.cook@foobar.com would all map to neil.cook@foobar.com. Use
  Lua modules such as LuaSQL-MySQL (luarocks install luasql-mysql) or
  LuaLDAP (luarocks install lualdap) to perform lookups in user
  databases. For example:

		setCanonicalize(canonicalize)

# GENERAL FUNCTIONS

The following functions are available anywhere; either as part of the 
configuration or within the allow/report/reset functions:

* lookupCountry(\<ComboAddress\>) - Returns the two-character country
  code of the country that the IP address is located in. A
  ComboAddress object can be created with the newCA() function. For example:
  
		my_country = lookupCountry(my_ca)

* newCA(\<IP[:port]\>) - Create and return an object representing an IP
  address (v4 or v6) and optional port. The object is called a
  ComboAddress. For example:
  
		my_ca = newCA("192.128.12.82")

* ComboAddress:tostring() - Return a string representing the IP
  address. For example:
  
		mystr = my_ca:tostring()

* newNetmaskGroup() - Return a NetmaskGroup object, which is a way to
  efficiently match IPs/subnets agagainst a range. For example:

		mynm = newNetmaskGroup()

* NetmaskGroup:addMask(\<cidr\>) - Add a mask to the NetmaskGroup, in
  cidr format. For example:

		mynm:addMask("182.22.0.0/16")

* NetmaskGroup:match(\<ip address\>) - Match an IP address against a
  NetmaskGroup. The IP address must be a ComboAddress object. For
  example:

		if (mynm.match(lt.remote)) 
		then
		    print("ip address matched") 
		end

* getDNSResolver(\<resolver name\>) - Return a DNS resolver object
  corresponding to the name specified. For example:
  
		resolv = getDNSResolver("MyResolver")

* Resolver:addResolver(\<ip address\>, \<port\>) - Adds the specified IP address
  and port as a recursive server to use when resolving DNS queries. For
  example:
  
		resolv = getDNSResolver("MyResolver")
		resolv:addResolver("8.8.8.8", 53)

* Resolver:setRequestTimeout(\<timeout\>) - Sets the timeout in
  milliseconds. For example
  
		resolv = getDNSResolver("MyResolver")
		resolv:setRequestTimeout(100)

* Resolver:setNumContexts(\<num contexts\>) - Sets the number of DNS
  contexts to use to perform DNS queries. Defaults to 12, but you may
  want to increase for performance, although it should not need to be
  higher than NumWorkerThreads. For example:

		resolv = getDNSResolver("MyResolver")
		resolv:setNumContexts(20)

* Resolver:lookupAddrByName(\<name\>, [\<num retries\>]) - Performs DNS A record resolution
  for the specified name, returning an array of IP address strings. Optionally
  the number of retries can be specified. For example:
  
		resolv = getDNSResolver("MyResolver")
  		resolv:lookupAddrByName("google.com")
	  	for i, addr in ipairs(dnames) do
      		    -- addr contains an IPv4 or IPv6 address
  		end

* Resolver:lookupNameByAddr(\<ComboAddress\>, [num retries]) - Performs
  DNS PTR record resolution for the specified address, returning an
  array of names.  Optionally the number of retries can be
  specified. For example:
  
		resolv = getDNSResolver("MyResolver")
  		resolv:lookupNameByAddr(lt.remote)
  		for i, dname in ipairs(dnames) do
  	    	    -- dname is the resolved DNS name
  		end

* Resolver:lookupRBL(\<ComboAddress\>, \<rbl zone\>, [num retries]) - Lookup an
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

* getStringStatsDB(\<stats db name\>) - Retrieve the StatsDB object specified by
  the name. For example:
  
		statsdb = getStringStatsDB("OneHourDB")

* StringStatsDB:twAdd(\<key\>, \<field name\>, \<value\>) - For the
  specified key, add the specified value to the specified field. Keys
  can be ComboAddress, strings or integers. Values can be
  ComboAddress, strings or integers. Some values only make sense for
  certain field types (e.g. adding a string to an int field type makes no
  sense). For example: 
  
		ca = newCA("192.168.1.2")
  		statsdb:twAdd("luser", "diffIPs", ca)
  		statsdb:twAdd(ca, "countLogins", 1)

* StringStatsDB:twSub(\<key\>, \<field name\>, \<value\>) - For the
  specified key, subtract the specified value from the specified field. Keys
  can be ComboAddress, strings or integers. Values can only be
  integers. For example:
  
		statsdb:twSub(ca, "countLogins", 1)

* StringStatsDB:twGet(\<key\>, \<field name\>, [\<value\>]) - For the
  specified key, retrieve the value for the specified field. For
  fields of type "countmin", specify the value you wish to
  retrieve. Return value is summed over all the windows in the db. For
  example:
  
		numLogins =  statsdb:twGet(lt.login, "countLogins")
  		numUSLogins = statsdb:twGet(lt.login, "countCountries", "US")

* StringStatsDB:twGetCurrent(\<key\>, \<field name\>, [\<value\>]) - For the 
  specified key, retrieve the value for the specified field for the
  current (latest) time window. For fields of type "countmin", specify
  the value you wish to retrieve. For example:
  
	      	numLogins = statsdb:twGetCurrent(lt.login, "countLogins")

* StringStatsDB:twGetWindows(\<key\>, \<field name\>, [\<value\>]) - For the 
  specified key, retrieve an array containing all the values for all
  the windows for the specified field. For fields of type "countmin",
  specify the value you wish to retrieve. For example:
  
		myarray = statsdb:twGetWindows(lt.login, "countCountries", "AU")

* StringStatsDB:twGetSize() - Returns the number of keys stored in the
  db. For example
  
		dbsize = statsdb:twGetSize()

* StringStatsDB:twReset(\<key\>) - Reset all stats for all windows in
  the db for the specified key. For example:
  
		statsdb:twReset(lt.login)

* infoLog(\<log string\>, \<key-value map\>) - Log at LOG_INFO level the
  specified string, adding "key=value" strings to the log for all the
  kvs specified in the key-value map. For example:
  
		infoLog("Logging is very important", { logging=1, foo=bar })

* warnLog(\<log string\>, \<key-value map\>) - Log at LOG_WARN level the
  specified string, adding "key=value" strings to the log for all the
  kvs specified in the key-value map. For example:
  
		warnLog("Logging is very important", { logging=1, foo=bar })

* errorLog(\<log string\>, \<key-value map\>) - Log at LOG_ERR level the
  specified string, adding "key=value" strings to the log for all the
  kvs specified in the key-value map. For example:
  
		errorLog("Logging is very important", { logging=1, foo=bar })

* blacklistIP(\<ip\>, \<expiry\>, \<reason string\>) - Blacklist the
  specified IP for expiry seconds, with the specified reason. IP
  address must be a ComboAddress. For example:
  
		blacklistIP(lt.remote, 300, "Attempted password brute forcing")

* blacklistLogin(\<login\>, \<expiry\> \<reason string\>) - Blacklist the
  specified login for expiry seconds, with the specified
  reason. For example: 
  
		blacklistLogin(lt.login, 300, "Potentially compromised account")

* blacklistIPLogin(\<ip\>, \<login\>, \<expiry\>, \<reason string\>) -
  Blacklist the specified IP-Login tuple for expiry seconds, with the
  specified reason. Only when that IP and login are received in the
  same login tuple will the request be blacklisted. IP address must be
  a ComboAddress. For example:
  
		blacklistIPLogin(lt.remote, lt.login, 300, "Account and IP are suspicious")

* LoginTuple - The only parameter to both the allow and report
  functions is a LoginTuple table. This table contains the following
  fields:
  
* LoginTuple.remote - An IP address of type ComboAddress,
  representing the system performing login.

* LoginTuple.login - The username of the user performing the login.
  		
* LoginTuple.pwhash - A hashed representation of the users password
  		
* LoginTuple.success - Whether the user login was successful.

* LoginTuple.policy_reject - If the login was not successful only because of a
  policy-based reject from wforce (i.e. the username and password were correct).

* LoginTuple.attrs - Additional array of (single valued) attributes
  about the login, e.g. information about the user from LDAP. For
  example:

		 for k, v in pairs(lt.attrs) do
			 if (k == "xxx")
			 then
				 -- do something
			 end
		 end
  		
* LoginTuple.attrs_mv - Additional array of (multi-valued)
  attributes about the login. For example:

		 for k, v in pairs(lt.attrs_mv) do
			 for i, vi in ipairs(v) do
				 if ((k == "xxx") and (vi == "yyy"))
				 then
					 -- do something
				 end
			 end
		 end

* LoginTuple.device_id - A string that represents the device that the
  user logged in from. Also see device_attrs.

* LoginTuple.device_attrs - Additional array of attributes about the
  device. For example:

		for k, v in pairs(lt.device_attrs) do
			 if (k == "name")
			 then
				 -- do something with v
			 end
		 end

# FILES
*/etc/wforce.conf*

# SEE ALSO
wforce(1) wforce_webhook(5)
