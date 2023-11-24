% WFORCE.CONF(5)
% Open-Xchange
% 2018

<!-- {% raw %} -->

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

* addCustomWebHook(\<custom webhook name\>, \<config key map\>) - Add
  a custom webhook, i.e. one which can be called from Lua (using
  "runCustomWebHook()" - see below) with arbitrary data, using the
  specified configuration keys. See *wforce_webhook(5)* for the
  supported config keys, and details of the HTTP(S) POST sent
  to webhook URLs. For example:

        config_keys={}
        config_keys["url"] = "http://webhooks.example.com:8080/webhook/"
        config_keys["secret"] = "verysecretcode"
        addCustomWebHook("mycustomhook", config_keys)

* addCustomStat(\<stat name\>) - Add a custom counter which can be
  used to track statistics. The stats for custom counters are logged
  every 5 minutes. The counter is incremented with the
  "incCustomStat" command. For example:

        addCustomStat("custom_stat1")

* setSiblings(\<list of IP/Host[:port[:protocol]]\>) - Set the list of siblings to which
  stats db and blacklist/whitelist data should be replicated. If port
  is not specified it defaults to 4001.  If protocol is not specified
  it defaults to udp. This function is safe to call while wforce is running. For example:
  
        setSiblings({"127.0.1.2", "sibling1.example.com:4004", "[::1]:4004:tcp"})

* setSiblingsWithKey(\<list of {IP/Host[:port[:protocol]], Encryption Key\>) - Identical
  to setSiblings() except that it allows an encryption key to be specified for
  each sibling. For example:

          setSiblingsWithKey({{"127.0.1.2", "Ay9KXgU3g4ygK+qWT0Ut4gH8PPz02gbtPeXWPdjD0HE="}, {"127.0.1.3:4004:tcp", "KaiQkCHloe2ysXv2HbxBAFqHI4N8+ahmwYwsbYlDdF0="}})

* addSibling(\<IP/Hostname[:port[:protocol]]\>) - Add a sibling to the list to which all
  stats db and blacklist/whitelist data should be replicated. Use [] to enclose
  IPv6 addresses. If port is not specified it defaults to 4001. If protocol is not specified
  it defaults to udp. This function is safe to call while wforce is running. For example:

        addSibling("192.168.1.23")
        addSibling("192.168.1.23:4001:udp")
        addSibling("192.168.1.23:4003:tcp")
        addSibling("[::1]:4003:tcp")
        addSibling("sibling1.example.com")

* addSiblingWithKey(\<IP[:port[:protocol]]\>, \<Encryption Key\>) - Identical 
  to addSibling(), except that an encryption key is specified to enable per-sibling
  encryption.For example:

          addSiblingWithKey("192.168.1.23", "Ay9KXgU3g4ygK+qWT0Ut4gH8PPz02gbtPeXWPdjD0HE=")

* removeSibling(\<IP/Host[:port[:protocol]]\>) - Remove a sibling to the list to which all
  stats db and blacklist/whitelist data should be replicated.  Use [] to enclose
  IPv6 addresses. If port
  is not specified it defaults to 4001. If protocol is not specified
  it defaults to udp. This function is safe to call while wforce is running. For example:

        removeSibling("192.168.1.23")
        removeSibling("sibling1.example.com:4001:udp")
        removeSibling("[::1]:4003:tcp")

* siblingListener(\<IP[:port]\>) - Listen for reports from siblings on
  the specified IP address and port.  If port is not specified
  it defaults to 4001. Wforce will always listen on both UDP and TCP
  ports. For example:
  
        siblingListener("0.0.0.0:4001")
        siblingListener("[::1]:4001")

* setSiblingConnectTimeout(\<timeout ms\>) - Sets a timeout in milliseconds 
  for new connections to siblings. Defaults to 5000 ms (5 seconds). For example:

        setSiblingConnectTimeout(1000)

* setMaxSiblingQueueSize(\<size\>) - Sets the maximum size of the
  send and receive queues for replication events waiting to be processed.
  Defaults to 5000. This is only to handle short-term spikes in load/latency -
  if error messages relating to the recv queue max size being reached are
  seen, then you should consider using sharded string stats dbs
  (newShardedStringStatsDB), and/or tuning the stats db expiry sleep
  time (twSetExpireSleep).

        setMaxSiblingQueueSize(10000)

* setNamedReportSinks(\<name\>, \<list of IP[:port]\>) - Set a named list
  of report sinks to which all received reports should be forwarded
  over UDP. Reports will be sent to the configured report sinks for a
  given name in a round-robin fashion if more than one is
  specified. Reports are sent separately to each named report sink. If
  port is not specified it defaults to 4501. Replaces the deprecated
  "setReportSinks()". For example: 
  
        setNamedReportSinks("logstash", {"127.0.1.2", "127.0.1.3:4501"})

* addNamedReportSink(\<name\>, \<IP[:port]\>) - Add a report sink to
  the named list to which all received reports should be forwarded
  over UDP. Reports will be sent to the configured report sinks for a
  given name in a round-robin fashion if more than one is
  specified. Reports are sent separately to each named report sink. If
  port is not specified it defaults to 4501. Replaces the deprecated
  "addReportSink()". For example:
  
        addNamedReportSink("logstash", "192.168.1.23")

* webserver(\<IP:port\>, \<password\>) - (*deprecated - see addListener() instead*) Listen for HTTP commands on the
  specified IP address and port. The password is used to authenticate client connections using basic authentication. 
  For example:
  
        webserver("0.0.0.0:8084", "super")

* addListener(\<IP:port\>, \<useSSL\>, \<cert file\>, \<key file\>, \<options\>) - (*replacement for webserver()*) Listen
  for HTTP commands on the specified IP address and port. If useSSL is true, then HTTPS must be used, and cert_file and
  key file are used, otherwise they are empty. Options contains a list of key value pairs to configure the TLS connection;
  these follow the command line option names in https://www.openssl.org/docs/manmaster/man3/SSL_CONF_cmd.html.
  For example, "min_protocol" to set the minimum TLS protocol version. You can add as many listeners as you choose. For example:

        addListener("0.0.0.0:8084", false, "", "", {})
        addListener("1.2.3.4:1234", true, "/etc/wforce/cert.pem", "/etc/wforce/key.pem", {minimum_protocol="TLSv1.2"})
        addListener("[::1]:9000", true, "/etc/wforce/cert.pem", "/etc/wforce/key.pem", {minimum_protocol="TLSv1.3"})

* setWebserverPassword(\<Password\>) - (*replacement for webserver password parameter*) Sets the basic authentication
  password for access to the webserver. This has been decoupled from the addListener() command because multiple listeners
  can now be created, which was not previously possible. For example:

        setWebserverPassword("foobar")

* controlSocket(\<IP[:port]\>) - Listen for control connections on the
  specified IP address and port. If port is not specified it defaults
  to 5199. For example: 
  
        controlSocket("0.0.0.0:4004")

* setKey(\<key\>) - Use the specified key for authenticating 
  connections from siblings. The key can be generated with makeKey()
  from the console. See *wforce(1)* for instructions on
  running a console client.
  Returns false if the key could not be set (e.g. invalid base64).
  For example:

        if not setKey("Ay9KXgU3g4ygK+qWT0Ut4gH8PPz02gbtPeXWPdjD0HE=")
            then
              ...

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
  the pool used to send webhook events. Defaults to 5 if not
  specified. For example:

        setNumWebHookThreads(2)

* setMaxWebserverConns(\<max conns\>) - Set the maximum number of
  active connections to the webserver. This can be used to limit the
  effect of too many queries to wforce. It defaults to 10,000. For
  example:

        setMaxWebserverConns(5000)

* setNumWebHookConnsPerThread(\<num conns\>) - Set the maximum number
  of connections used by each WebHook thread. Defaults to 10 if not
  specified. This setting replaces the deprecated "num_conns" per-hook
  configuration setting. For example:

        setNumWebHookConnsPerThread(50)

* setWebHookQueueSize(\<queue size\>) - Set the size of the queue for
  webhook events. If the queue gets too big, then webhooks will be
  discarded, and an error will be logged. The default queue size is
  50000, which should be appropriate for most use-cases.

        setWebHookQueueSize(100000)

* setWebHookTimeoutSecs(\<timeout secs\>) - Set the maximum time a
  request can take for webhooks. For example:

        setWebHookTimeoutSecs(2)

* newGeoIP2DB(\<db name\>, \<filename\>) - Opens and initializes a
  GeoIP2 database. A name must be chosen, and the filename of the
  database to open must also be supplied. To obtain an object allowing
  lookups to be performed against the database, use the getGeoIP2DB()
  function. For example:

        newGeoIP2DB("CityDB", "/usr/share/GeoIP/GeoLite2-City.mmdb")

* initGeoIPDB() - (Deprecated - use newGeoIP2DB()) Initializes the
  country-level IPv4 and IPv6 GeoIP Legacy databases. If either of
  these databases is not installed, this command will fail and wforce
  will not start. For example:
  
        initGeoIPDB()

* initGeoIPCityDB() - (Deprecated - use newGeoIP2DB()) Initializes the
  city-level IPv4 and IPv6 GeoIP Legacy databases. If either of these
  databases is not installed, this command will fail and wforce will
  not start. Ensure these databases have the right names if you're
  using the free/lite DBs - you may need to create symbolic links
  e.g. GeoIPCityv6.dat -> GeoLiteCityv6.dat. For example:
  
        initGeoIPCityDB()

* initGeoIPISPDB() - (Deprecated - use newGeoIP2DB()) Initializes the
  ISP-level IPv4 and IPv6 GeoIP Legacy databases. If either of these
  databases is not installed, this command will fail and wforce will
  not start. For example:
  
        initGeoIPISPDB()

* newDNSResolver(\<resolver name\>) - Create a new DNS resolver object with the
  specified name. Note this does not return the resolver object - that
  is achieved with the getDNSResolver() function. For example:
  
        newDNSResolver("MyResolver")

* setHLLBits(\<num bits\>) - Set the accuracy of the HyperLogLog
  algorithm. The value can be between 4-30, with a high number being
  more accurate at the expense of using (potentially a lot) more
  memory. The default value is 6. If you require more accuracy,
  consider increasing slightly, but check your memory usage carefully.

* setCountMinBits(\<eps\>, \<gamma\>) - Set the accuracy of the
  CountMin algorithm. The value of eps can be between 0.01 and 1, with
  a lower number giving more accuracy at the expense of memory. The
  default is 0.05. The value of gamma is between 0 and 1, with a
  higher number giving higher accuracy. The default for gamma
  is 0.2. If you require more accuracy, consider changing these values
  slightly, but check your memory usage carefully.

* newStringStatsDB(\<stats db name\>, \<window size\>, \<num windows\>,
  \<field map\>) - Create a new StatsDB object with
  the specified name. Note this does not return the object - that is
  achieved with the getStringStatsDB() function. For example:
  
        field_map = {}
        field_map["countLogins"] = "int"
        field_map["diffPasswords"] = "hll"
        field_map["countCountries"] = "countmin"
        newStringStatsDB("OneHourDB", 900, 4, field_map)

* newShardedStringStatsDB(\<stats db name\>, \<window size\>, \<num windows\>,
  \<field map\>, \<num shards\>) - Identical to "newStringStatsDB"
  except that it creates a sharded DB, which is more scalable at
  higher query loads. A good starting value for the number of shards
  might be 10.

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

* disableBuiltinBlacklists() - Disable the built-in blacklisting checks,
  enabling them to be checked from Lua instead. For example:

        disableBuiltinBlacklists()

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
  you use a single redis instance for all wforce servers,
  then you should not specify this option, as it will cause
  unnecessary writes to the redis DB. For example:
  
        blacklistPersistReplicated()

* blacklistRedisUsername(<username>) - Set the Redis username for authentication
  to the redis DB. For example:

        blacklistRedisUsername("foobar")

* blacklistRedisPassword(<password>) - Set the Redis password for authentication
  to the redis DB. For example:

        blacklistRedisPassword("secret")

* blacklistPersistConnectTimeout(<timeout secs>) - Set the connect
  timeout for connecting to the persistent redis DB. If the timeout is
  exceeded during connection at startup then wforce will exit,
  otherwise during normal operation if the timeout is exceeded, an
  error will be logged. For example:

        blacklistPersistConnectTimeout(2)

* blacklistPersistRWTimeout(<timeout secs>, <timeout usecs>) - Set the
  timeout for reading from/writing to the Redis DB. For example:

        blacklistPersistRWTimeout(0, 50000)

* disableBuiltinWhitelists() - Disable the built-in whitelisting checks,
  enabling them to be checked from Lua instead. For example:

        disableBuiltinWhitelists()

* whitelistPersistDB(\<ip\>, \<port\>) - Make the whitelist persistent
  by storing entries in the specified redis DB. The IP address is a
  string, and port should be 6379 unless you are running redis
  on a non-standard port. If this option is specified, wforce will
  read all the whitelist entries from the redis DB on startup. For example:
  
        whitelistPersistDB("127.0.0.1", 6379)

* whitelistRedisUsername(<username>) - Set the Redis username for authentication
  to the redis DB. For example:

        whitelistRedisUsername("foobar")

* whitelistRedisPassword(<password>) - Set the Redis password for authentication
  to the redis DB. For example:

        whitelistRedisPassword("secret")

* whitelistPersistReplicated() - Store whitelist entries that have
  been replicated in the redis DB. By default, replicated whitelist 
  entries will not be persisted. If you use a local
  redis DB for each wforce server, then use this config option. If
  you use a single redis instance for all wforce servers,
  then you should not specify this option, as it will cause
  unnecessary writes to the redis DB. For example:
  
        whitelistPersistReplicated()

* whitelistPersistConnectTimeout(<timeout secs>) - Set the connect
  timeout for connecting to the persistent redis DB. If the timeout is
  exceeded during connection at startup then wforce will exit,
  otherwise during normal operation if the timeout is exceeded, an
  error will be logged. For example:

        whitelistPersistConnectTimeout(2)

* whitelistPersistRWTimeout(<timeout secs>, <timeout usecs>) - Set the
  timeout for reading from/writing to the Redis DB. For example:

        whitelistPersistRWTimeout(0, 50000)

* setBlacklistIPRetMsg(<msg>) - Set the message to be returned to
  clients whose IP address is blacklisted. The strings "{ip}" and
  "{login}" will be substituted for the actual IP address and login name. 
  For example:

        setBlacklistIPRetMsg("Go away your IP {ip} is blacklisted")

* setBlacklistLoginRetMsg(<msg>) - Set the message to be returned to
  clients whose login is blacklisted. The strings "{ip}" and
  "{login}" will be substituted for the actual IP address and login name. For example:

        setBlacklistLoginRetMsg("Go away your login {login} is blacklisted")

* setBlacklistIPLoginRetMsg(<msg>) - Set the message to be returned to
  clients whose IP address/login is blacklisted. The strings "{ip}" and
  "{login}" will be substituted for the actual IP address and login name. For example:

        setBlacklistIPLoginRetMsg("Go away your IP {ip}/Login {login} is blacklisted")

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

* setCustomEndpoint(\<name of endpoint\>, \<boolean\>, \<custom lua function\>) -
  Create a new custom REST endpoint with the given name, which when
  invoked will call the supplied custom lua function. This allows
  admins to arbitrarily extend the wforce REST API with new REST
  endpoints. Admins can create as many custom endpoints as they
  require. Custom endpoints can only be accessed via a POST method,
  and all arguments as passed as key-value pairs of a top-level
  "attrs" json object (these will be split into two tables - one for
  single-valued attrs, and the other for multi-valued attrs - see
  CustomFuncArgs below). If the boolean argument is true,
  then all arguments to the custom function will be sent to all
  configured named report sinks. Return information is passed with a
  boolean "success" and "r_attrs" json object containing return
  key-value pairs. See wforce.conf.example for an example. For
  example: 

        function custom(args)
          for k,v in pairs(args.attrs) do
            infoLog("custom func argument attrs", { key=k, value=v });
          end
          -- return consists of a boolean, followed by { key-value pairs }
          return true, { key=value }
        end
        setCustomEndpoint("custom", false, custom)
        -- Set boolean to true to enable arguments to be sent to all
        -- report sinks
        -- setCustomEndpoint("custom", true, custom)

* setCustomGetEndpoint(\<name of endpoint\>, \<custom lua
  function\>) - Create a new custom REST endpoint accessible via a GET
  command (setCustomEndpoint() only works with POST). The return value
  of the function is a string, which will be passed to the HTTP client
  as a text/plain response body. For example:

        function textIPBlacklist()
            local ipbl = getIPBlacklist()
            local ret_table = {}
            for i,j in pairs(ipbl)
            do
                for k,v in pairs(j)
                do
                    if k == "ip"
                    then
                        table.insert(ret_table, v)
                    end
                end
            end
            local s =  table.concat(ret_table, "\n") .. "\n"
            return s
        end

        setCustomGetEndpoint("textIPBlacklist", textIPBlacklist)

* setVerboseAllowLog() - When logging allow requests, for performance
  reaons, allow requests returning 0 will not be logged by default. In
  order to log allow requests returning 0, use this function. For
  example:

        setVerboseAllowLog()

* addSyncHost(\<sync host address\>, \<sync host password\>,
  \<replication address\>, \<callback address\>) - If you wish wforce
  to synchronize the contents of its StatsDBs with the rest of a
  cluster, then use this configuration command. If any sync hosts are
  added, wforce will attempt to contact them in turn and find a host
  which has been up longer than the number of seconds specified in
  setMinSyncHostUptime(). The sync host address should include a port
  (it defaults to 8084). If successful, the sync host will send the
  contents of its StatsDBs to this wforce instance on the replication
  address specified (this address should have the same port as
  configured in siblingListener() and defaults to 4001), and when it
  is finished it will notify this instance of wforce using the
  callback address (this address should have the same port as
  configured in webserver and defaults to 8084). N.B. It should be noted
  that the encryption key of the local instance is sent to the sync host,
  so to protect the confidentiality of the key, you may want to consider
  running an HTTPS reverse proxy in front of the sync host. For
  example:

        -- Add 10.2.3.1:8084 as a sync host,
        -- and use the password "super"
        -- Send the DB dump to 10.2.1.1:4001
        -- and let me know on 10.2.1.1:8084 when the dump is finished
        addSyncHost("10.2.3.1:8084", "super", "10.2.1.1:4001", "10.2.1.1:8084") 
        -- Add https://10.2.3.1:8084 as a sync host,
        -- and use the password "super"
        -- Send the DB dump to https://10.2.1.1:4001
        addSyncHost("https://host1.example.com:8084", "super", "10.2.1.1:4001", "https://host2.example.com:8084") 

* setMinSyncHostUptime(\<seconds\>) - The minimum time that any sync
  host must have been up for it to be able to send me the contents of
  its DBs. Defaults to 3600. For example:

        setMinSyncHostUptime(1800)

* disableCurlPeerVerification() - Disable checking of peer certificates in all outbound HTTPS connections. This is not recommended except for debugging or development purposes. For example:

        disableCurlPeerVerification()

* disableCurlHostVerification() - Disable checking of the hostname in the peer certificate for all outbound HTTPS connections. This is not recommended except for debugging or development purposes.

        disableCurlHostVerification()

* setCurlCABundleFile(\<Path to CA File\>) - Gives the location of a file containing the certificate authorities to use for checking HTTPS server certificates. Use this if the standard installed root certs do not contain the certs you need. This should be a file containing 1:N certs in PEM format.

        setCurlCABundleFile("/etc/ca/local_cas.pem")

* setCurlClientCertAndKey(\<Path to Cert File\>, \<Path to Key File\>) - Gives the location of the certificate and key files to use for mutual TLS authentication (in PEM format).

        setCurlClientCertAndKey("/etc/certs/clientcert.pem", "/etc/certs/clientkey.pem")

* setMetricsNoPassword() - Disable password protection for the /metrics endpoint.

# GENERAL FUNCTIONS

The following functions are available anywhere; either as part of the 
configuration or within the allow/report/reset functions:

* getGeoIP2DB(\<db name\>) - Return an object which can be used to
  perform GeoIP lookups. The database must first be initialised using
  newGeoIP2DB(). For example:

        local citydb = getGeoIP2DB("CityDB")

* GeoIP2DB:lookupCountry(\<ComboAddress\>) - Returns the two-character
  country code of the country that the IP address is located in. A
  ComboAddress object can be created with the newCA() function. For
  example: 
  
		my_country = countrydb:lookupCountry(newCA("8.8.8.8"))
        my_country = countrydb:lookupCountry(lt.remote)

* GeoIP2DB:lookupISP(\<ComboAddress\>) - Returns the name of the ISP hosting
  the IP address. A ComboAddress object can be created with the
  newCA() function. For example:

		local my_isp = ispdb:lookupISP(newCA("128.243.16.21"))

* GeoIP2DB:lookupCity(\<ComboAddress\>) - Returns a map containing information
  about the IP address, such as the city name and latitude and
  longitude. See GeoIPRecord below for the full list of fields. For
  example:

		local gip_record = citydb:lookupCity(lt.remote)
		local my_city = gip_record.city
		local my_latitude = gip_record.latitude

* GeoIP2DB:lookupStringValue(\<ComboAddress\>, \<array of string
  values\>) - Returns a string corresponding to the value of the field
  specified by the array of string values. For example:

        local city_name = citydb:lookupStringValue(newCA(ip_address), {"city", "names", "en"})

* GeoIP2DB:lookupUIntValue(\<ComboAddress\>, \<array of string
  values\>) - Returns an integer corresponding to the value of the field
  specified by the array of string values. For example:

        local accuracy = citydb:lookupUIntValue(newCA(ip_address), {"location", "accuracy_radius"})

* GeoIP2DB:lookupBoolValue(\<ComboAddress\>, \<array of string
  values\>) - Returns a boolean corresponding to the value of the field
  specified by the array of string values. For example:

        local eu = citydb:lookupBoolValue(newCA(ip_address), {"country", "is_in_european_union"})

* GeoIP2DB:lookupDoubleValue(\<ComboAddress\>, \<array of string
  values\>) - Returns the value corresponding to the value of the field
  specified by the array of string values (which can be either double
  or float in the MMDB specification). For example:

        local city_lat = citydb:lookupDoubleValue(newCA(ip_address), {"location", "latitude"})
        local city_long = citydb:lookupDoubleValue(newCA(ip_address), {"location", "longitude"})

* lookupCountry(\<ComboAddress\>) - (Deprecated - use the new GeoIP2
  function). Returns the two-character country
  code of the country that the IP address is located in. A
  ComboAddress object can be created with the newCA() function. For
  example: 
  
		my_country = lookupCountry(my_ca)

* lookupISP(\<ComboAddress\>) - (Deprecated - use the new GeoIP2
  function). Returns the name of the ISP hosting
  the IP address. A ComboAddress object can be created with the
  newCA() function. For example:

		local my_isp = lookupISP(newCA("128.243.16.21"))

* lookupCity(\<ComboAddress\>) - (Deprecated - use the new GeoIP2
  function). Returns a map containing information
  about the IP address, such as the city name and latitude and
  longitude. See GeoIPRecord below for the full list of fields. For
  example:

		local gip_record = lookupCity(lt.remote)
		local my_city = gip_record.city
		local my_latitude = gip_record.latitude

* newCA(\<IP[:port]\>) - Create and return an object representing an IP
  address (v4 or v6) and optional port. The object is called a
  ComboAddress. For example:
  
		my_ca = newCA("192.128.12.82")

* ComboAddress:tostring() - Return a string representing the IP
  address. For example:
  
		mystr = my_ca:tostring()

* newNetmask(\<IP[/mask]\>) - Create and return an object representing
a Netmask. For example:

		my_nm = newNetmask("8.0.0.0/8")

* newNetmaskGroup() - Return a NetmaskGroup object, which is a way to
  efficiently match IPs/subnets against a range. For example:

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

* runCustomWebHook(\<custom webhook name\>, \<webhook data\>) - Run a
  previously configured custom webhook, using the supplied webhook
  data. By default the Content-Type of the data is "application/json",
  however this can be changed using the "content-type" config key when
  configuring the custom webhook. Custom webhooks are run using the
  same thread pool as normal webhooks. For example:

		runCustomWebHook(mycustomhook", "{ \"foo\":\"bar\" }")

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

* StringStatsDB:twResetField(\<key\>, \<field name\>) - Reset all
  stats for all windows in the db for the specified key and field
  name. For example:
  
		statsdb:twResetField(lt.login, "countLogins")

* StringStatsDB:twSetExpireSleep(\<milliseconds\>) - Set the sleep
  interval between checks to expire/expunge entries. Defaults to
  250ms. For example:

        statsdb:twSetExpireSleep(200)

* infoLog(\<log string\>, \<key-value map\>) - Log at LOG_INFO level t<he
  specified string, adding "key=value" strings to the log for all the
  kvs specified in the key-value map. For example:
  
		infoLog("Logging is very important", { logging=1, foo=bar })

* vinfoLog(\<log string\>, \<key-value map\>) - Log at LOG_INFO level the
  specified string, adding "key=value" strings to the log for all the
  kvs specified in the key-value map, but only if wforce was started
  with the (undocumented) -v flag (for verbose). For example:
  
		vinfoLog("Logging is very important", { logging=1, foo=bar })

* warnLog(\<log string\>, \<key-value map\>) - Log at LOG_WARN level the
  specified string, adding "key=value" strings to the log for all the
  kvs specified in the key-value map. For example:
  
		warnLog("Logging is very important", { logging=1, foo=bar })

* errorLog(\<log string\>, \<key-value map\>) - Log at LOG_ERR level the
  specified string, adding "key=value" strings to the log for all the
  kvs specified in the key-value map. For example:
  
		errorLog("Logging is very important", { logging=1, foo=bar })

* debugLog(\<log string\>, \<key-value map\>) - Log at LOG_DEBUG level the
  specified string, adding "key=value" strings to the log for all the
  kvs specified in the key-value map, but only if wforce was started
  with the (undocumented) -v flag (for verbose). For example:
  
		debugLog("This will only log if wforce is started with -v", { logging=1, foo=bar })

* getBlacklistIPRetMsg() - Get the message to be returned to
  clients whose IP address is blacklisted. For example:

        local retmsg = getBlacklistIPRetMsg()

* getBlacklistLoginRetMsg() - Get the message to be returned to
  clients whose login is blacklisted. For example:

        local retmsg = getBlacklistLoginRetMsg()

* getBlacklistIPLoginRetMsg() - Get the message to be returned to
  clients whose IP address/login is blacklisted. For example:

        local retmsg = getBlacklistIPLoginRetMsg()

* blacklistNetmask(\<Netmask\>, \<expiry\>, \<reason string\>) - Blacklist the
  specified netmask for expiry seconds, with the specified reason. Netmask
  address must be a Netmask object, e.g. created with newNetmask(). For example:
  
		blacklistNetmask(newNetmask("12.32.0.0/16"), 300, "Attempted password brute forcing")

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

* unblacklistNetmask(\<Netmask\>) Remove the blacklist for the
  specified netmask. Netmask address must be a Netmask object,
  e.g. created with newNetmask(). For example:
  
		unblacklistNetmask(newNetmask("12.32.0.0/16"))

* unblacklistIP(\<ip\>) - Remove the blacklist for the specified
  IP. IP address must be a ComboAddress. For example:
  
		unblacklistIP(lt.remote)

* unblacklistLogin(\<login\>) - Remove the blacklist for the specified
  login. For example:
  
		unblacklistLogin(lt.login)

* unblacklistIPLogin(\<ip\>, \<login\>) - Remove the blacklist for the specified
  IP-Login tuple. IP address must be a ComboAddress. For example:
  
		unblacklistIPLogin(lt.remote, lt.login)

* checkBlacklistIP(\<ip\>) - Check if an IP is blacklisted. Return
  true if the IP is blacklisted. IP must be a ComboAddress. For example:

        checkBlacklistIP(newCA("192.1.2.3"))

* checkBlacklistLogin(\<login\>) - Check if a login is
  blacklisted. Return true if the login is blacklisted. For example:

        checkBlacklistLogin(lt.login)

* checkBlacklistIPLogin(\<ip\>, \<login\>) - Check if a IP/login is
  blacklisted. Return true if the ip/login tuple is blacklisted. For
  example:

        checkBlacklistIPLogin(lt.remote, lt.login)

* whitelistNetmask(\<Netmask\>, \<expiry\>, \<reason string\>) - Whitelist the
  specified netmask for expiry seconds, with the specified reason. Netmask
  address must be a Netmask object, e.g. created with newNetmask(). For example:
  
		whitelistNetmask(newNetmask("12.32.0.0/16"), 300, "Attempted password brute forcing")

* whitelistIP(\<ip\>, \<expiry\>, \<reason string\>) - Whitelist the
  specified IP for expiry seconds, with the specified reason. IP
  address must be a ComboAddress. For example:
  
		whitelistIP(lt.remote, 300, "Attempted password brute forcing")

* whitelistLogin(\<login\>, \<expiry\> \<reason string\>) - Whitelist the
  specified login for expiry seconds, with the specified
  reason. For example: 
  
		whitelistLogin(lt.login, 300, "Potentially compromised account")

* whitelistIPLogin(\<ip\>, \<login\>, \<expiry\>, \<reason string\>) -
  Whitelist the specified IP-Login tuple for expiry seconds, with the
  specified reason. Only when that IP and login are received in the
  same login tuple will the request be whitelisted. IP address must be
  a ComboAddress. For example:
  
		whitelistIPLogin(lt.remote, lt.login, 300, "Account and IP are suspicious")

* unwhitelistNetmask(\<Netmask\>) Remove the whitelist for the
  specified netmask. Netmask address must be a Netmask object,
  e.g. created with newNetmask(). For example:
  
		unwhitelistNetmask(newNetmask("12.32.0.0/16"))

* unwhitelistIP(\<ip\>) - Remove the whitelist for the specified
  IP. IP address must be a ComboAddress. For example:
  
		unwhitelistIP(lt.remote)

* unwhitelistLogin(\<login\>) - Remove the whitelist for the specified
  login. For example:
  
		unwhitelistLogin(lt.login)

* unwhitelistIPLogin(\<ip\>, \<login\>) - Remove the whitelist for the specified
  IP-Login tuple. IP address must be a ComboAddress. For example:
  
		unwhitelistIPLogin(lt.remote, lt.login)

* checkWhitelistIP(\<ip\>) - Check if an IP is whitelisted. Return
  true if the IP is whitelisted. IP must be a ComboAddress. For example:

        checkWhitelistIP(newCA("192.1.2.3"))

* checkWhitelistLogin(\<login\>) - Check if a login is
  whitelisted. Return true if the login is whitelisted. For example:

        checkWhitelistLogin(lt.login)

* checkWhitelistIPLogin(\<ip\>, \<login\>) - Check if a IP/login is
  whitelisted. Return true if the ip/login tuple is whitelisted. For
  example:

        checkWhitelistIPLogin(lt.remote, lt.login)

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

* LoginTuple.device_id - An optional string that represents the device
  that the user logged in from. Also see device_attrs.

* LoginTuple.device_attrs - Additional array of attributes about the
  device, which is parsed from the device_attrs string. The protocol
  string is used to determine how to parse device_id, so that MUST
  also be present. For all protocols, the following keys are set
  wherever possible: os.family, os.major, os.minor. For http, the
  following additional keys are set wherever possible: device.family,
  device.model, device.brand, browser.family, browser.major,
  browser.minor. For imap, the following additional keys are set
  wherever possible: imapc.family, imapc.major, imapc.minor. For
  mobileapi, the following additional keys are set: app.name,
  app.brand, app.major, app.minor, device.family. For example:

		if (lt.device_attrs["os.family"] == "Mac OS X")
		then
		    -- do something special for macOS
		end

* LoginTuple.protocol - A string representing the protocol that was
  used to access mail, i.e. http, imap, pop3, mobileapi
  etc. LoginTuple.protocol MUST be set in order to parse
  device_id into device_attrs, however currently only http, imap  and
  mobileapi are recognized protocols when parsing device_id. For
  example:

		if (lt.protocol == "http")
		then
			-- do something
		end

* LoginTuple.tls - A boolean representing whether the login session
  used TLS or not. If the client is using TLS offload proxies then it
  may be set to false.

* LoginTuple.session_id - An optional string representing the
  particular session that a user used to login. If this is not
  supplied by the wforce client, it will be an empty string.

* GeoIPRecord - The type returned by the lookupCity() function. See
  below for fields:

* GeoIPRecord.country_code - The two-letter country code e.g. "US".

* GeoIPRecord.country_name - The country name, e.g. "United States"

* GeoIPRecord.region - The region, e.g. "CA"

* GeoIPRecord.city - The city name, e.g. "Mountain View"

* GeoIPRecord.postal_code - The postal code, e.g. "93102" or "BA216AS"

* GeoIPRecord.latitude - The latitude, e.g. 37.386001586914

* GeoIPRecord.longitude - The longitude, e.g. -122.08380126953

* CustomFuncArgs - The only parameter to custom functions
  is a CustomFuncArgs table. This table contains the following fields:

* CustomFuncArgs.attrs - Array of (single valued) attributes supplied
  by the caller. For example:

		 for k, v in pairs(args.attrs) do
			 if (k == "xxx")
			 then
				 -- do something
			 end
		 end
  		
* CustomFuncArgs.attrs_mv - Array of (multi-valued)
  attributes supplied by the caller. For example:

		 for k, v in pairs(args.attrs_mv) do
			 for i, vi in ipairs(v) do
				 if ((k == "xxx") and (vi == "yyy"))
				 then
					 -- do something
				 end
			 end
		 end

* incCustomStat(\<stat name\>) - Increment a custom statistics
  counter. The value of the counter will be logged every 5 minutes,
  and then reset. For example:

        incCustomStat("custom_stat1")

# FILES
*/etc/wforce/wforce.conf*

# SEE ALSO
wforce(1) wforce_webhook(5)

<!-- {% endraw %} -->
