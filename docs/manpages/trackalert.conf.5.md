% TRACKALERT.CONF(5)
% Open-Xchange
% 2018

# NAME
**trackalert.conf** - configuration file for trackalert daemon

# DESCRIPTION
This file is read by **trackalert** and is a Lua script containing Lua
commands to implement (a) configuration of the trackalert daemon and
(b) functions that control the operation of the trackalert daemon in
response to HTTP commands, specifically "report".

An alternative version of this file can be specified with

    trackalert -C private_trackalert.conf ...

# CONFIGURATION-ONLY FUNCTIONS

The following functions are for configuration of trackalert only, and
cannot be called inside the report or background functions:

* setACL(\<list of netmasks\>) - Set the access control list for the
  HTTP Server. For example, to allow access from any IP address, specify:
  
        setACL({"0.0.0.0/0"}) 

* addACL(\<netmask\>) - Add a netmask to the access control list for the
  HTTP server. For example, to allow access from 127.0.0.0/8, specify:
  
        addACL("127.0.0.0/8")

* addCustomWebHook(\<custom webhook name\>, \<config key map\>) - Add
  a custom webhook, i.e. one which can be called from Lua (using
  "runCustomWebHook()" - see below) with arbitrary data, using the
  specified configuration keys. See *trackalert_webhook(5)* for the
  supported config keys, and details of the HTTP(S) POST sent
  to webhook URLs. For example:

        config_keys={}
        config_keys["url"] = "http://webhooks.example.com:8080/webhook/"
        config_keys["secret"] = "verysecretcode"
        addCustomWebHook("mycustomhook", config_keys)

* addCustomStat(\<stat name\>) - Add a custom counter which can be
  used to track statistics. The stats for custom counters are logged
  every 5 minutes. The counter is incremented with the
  "incCustomCounter" command. For example:

        addCustomStat("custom_stat1")

* webserver(\<IP:port\>, \<password\>) - (*deprecated - see addListener() instead*) Listen for HTTP commands on the
  specified IP address and port. The password is used to authenticate
  client connections using basic authentication. For example:
  
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
  connections from siblings. The key must be generated with makeKey()
  from the console. See *trackalert(1)* for instructions on
  running a console client.
  Returns false if the key could not be set (e.g. invalid base64).
  For example:

        if not setKey("Ay9KXgU3g4ygK+qWT0Ut4gH8PPz02gbtPeXWPdjD0HE=")
            then
              ...

* setNumLuaStates(\<num states\>) - Set the number of Lua Contexts that
  will be created to run report commands. Defaults to 10
  if not specified. See also setNumReportThreads() and
  setNumSchedulerThreads(). Should be at least equal to
  NumReportThreads \+ NumSchedulerThreads. For example:
  
        setNumLuaStates(10)

* setNumWorkerThreads(\<num threads\>) - Set the number of threads in
  the pool used to receive webserver commands. Note that the report
  function itself is run in a separate report thread pool controlled
  by setNumReportThreads(). Defaults to 4 if not specified. For
  example:

        setNumWorkerThreads(4)

* setMaxWebserverConns(\<max conns\>) - Set the maximum number of
  active connections to the webserver. This can be used to limit the
  effect of too many queries to trackalert. It defaults to 10,000. For
  example:

        setMaxWebserverConns(5000)

* setNumReportThreads(\<num threads\>) - Set the number of threads in
  the pool used to run Lua report functions. Each thread uses a
  separate Lua Context, (see setNumLuaStates()). Defaults to 6 if not
  specified. For example:
  
        setNumReportThreads(6)

* setNumSchedulerThreads(\<num threads\>) - Set the number of threads
  in the pool used to run scheduled background Lua functions. Each
  thread uses a separate Lua Context, the number of which is set with
  setNumLuaStates(). Defaults to 4 if not specified. For example:
  
        setNumSchedulerThreads(2)

* setNumWebHookThreads(\<num threads\>) - Set the number of threads in
  the pool used to send webhook events. Defaults to 4 if not
  specified. For example:

        setNumWebHookThreads(2)

* newGeoIP2DB(\<db name\>, \<filename\>) - Opens and initializes a
  GeoIP2 database. A name must be chosen, and the filename of the
  database to open must also be supplied. To obtain an object allowing
  lookups to be performed against the database, use the getGeoIP2DB()
  function. For example:

        newGeoIP2DB("CityDB", "/usr/share/GeoIP/GeoLite2-City.mmdb")

* initGeoIPDB() - (Deprecated - use newGeoIP2DB()). Initializes the
  country-level IPv4 and IPv6 GeoIP databases. If either of these
  databases is not installed, this command will fail and trackalert
  will not start. For example:
  
        initGeoIPDB()

* initGeoIPCityDB() - (Deprecated - use newGeoIP2DB()). Initializes
the city-level IPv4 and IPv6 GeoIP databases. If either of these
databases is not installed, this command will fail and trackalert will
not start. Ensure these databases have the right names if you're using
the free/lite DBs - you may need to create symbolic links
e.g. GeoIPCityv6.dat -> GeoLiteCityv6.dat. For example:
  
        initGeoIPCityDB()

* initGeoIPISPDB() - (Deprecated - use newGeoIP2DB()). Initializes the
  ISP-level IPv4 and IPv6 GeoIP databases. If either of these
  databases is not installed, this command will fail and trackalert
  will not start. For example:
  
        initGeoIPISPDB()

* setNumWebHookThreads(\<num threads\>) - Set the number of threads in
  the pool used to send webhook events. Defaults to 5 if not
  specified. For example:

        setNumWebHookThreads(2)

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

* setReport(\<report func\>) - Tell trackalert to use the specified
  Lua function for handling all "report" commands. For example:
  
        setReport(report)

* setBackground(\<name\>, \<lua function\> - The setBackground
  function registers a background function with a given name. The name
  is used when scheduling the function using the
  xxxScheduleBackgroundFunc() functions.

        setBackground("mybg", backgroundFunc)

* cronScheduleBackgroundFunc(\<cron string\>, \<background function
  name\>) - Tells trackalert to run the specified function according
  to the given cron schedule (note the
  name given in setBackground() is used, *not* the actual function
  name). Note that cron ranges are not currently supported - if you
  want to schedule the same function to run for example on two
  different days of the week, then you would use two different calls
  to this function to achieve that. For example:

        cronScheduleBackgroundFunc("0 0 1 * *", "mybg")
        cronScheduleBackgroundFunc("0 0 6 * *", "mybg")

* intervalScheduleBackgroundFunc(\<duration string\>, \<background
  function name\>) - Tells trackalert to run the specified function
  at the interval given in duration string. The duration string is of
  the format h[h][:mm][:ss], so for example "10" would indicate an
  interval of 10 hours, "00:00:01" would indicate an interval of 1
  second, and "5:01" would indicate an interval of 5 hours and 1
  minute. For example:

        intervalScheduleBackgroundFunc("00:30:00", "mybg") 
        intervalScheduleBackgroundFunc("24", "mybg") 

* setCustomEndpoint(\<name of endpoint\>, \<custom lua function\>) -
  Create a new custom REST endpoint with the given name, which when
  invoked will call the supplied custom lua function. This allows
  admins to arbitrarily extend the trackalert REST API with new REST
  endpoints. Admins can create as many custom endpoints as they
  require. Custom endpoints can only be accessed via a POST method,
  and all arguments are passed as key-value pairs of a top-level
  "attrs" json object (these will be split into two tables - one for
  single-valued attrs, and the other for multi-valued attrs - see
  CustomFuncArgs below). Return information is passed with a
  boolean "success" and "r_attrs" json object containing return
  key-value pairs. For example:

        function custom(args)
          for k,v in pairs(args.attrs) do
            infoLog("custom func argument attrs", { key=k, value=v });
          end
          -- return consists of a boolean, followed by { key-value pairs }
          return true, { key=value }
        end
        setCustomEndpoint("custom", custom)

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

* runCustomWebHook(\<custom webhook name\>, \<webhook data\>) - Run a
  previously configured custom webhook, using the supplied webhook
  data. By default the Content-Type of the data is "application/json",
  however this can be changed using the "content-type" config key when
  configuring the custom webhook. Custom webhooks are run using the
  same thread pool as normal webhooks. For example:

		runCustomWebHook(mycustomhook", "{ \"foo\":\"bar\" }")

* infoLog(\<log string\>, \<key-value map\>) - Log at LOG_INFO level the
  specified string, adding "key=value" strings to the log for all the
  kvs specified in the key-value map. For example:
  
		infoLog("Logging is very important", { logging=1, foo=bar })

* vinfoLog(\<log string\>, \<key-value map\>) - Log at LOG_INFO level the
  specified string, adding "key=value" strings to the log for all the
  kvs specified in the key-value map, but only if trackalert was
  started with the (undocumented) -v flag (for verbose). For example:
  
		vinfoLog("Logging is very important", { logging=1, foo=bar })

* warnLog(\<log string\>, \<key-value map\>) - Log at LOG_WARN level the
  specified string, adding "key=value" strings to the log for all the
  kvs specified in the key-value map. For example:
  
		warnLog("Logging is very important", { logging=1, foo=bar })

* errorLog(\<log string\>, \<key-value map\>) - Log at LOG_ERR level the
  specified string, adding "key=value" strings to the log for all the
  kvs specified in the key-value map. For example:
  
		errorLog("Logging is very important", { logging=1, foo=bar })

* debugLog(\<log string\>, \<key-value map\>) - Log at LOG_DEBUG level
  the specified string, adding "key=value" strings to the log for all
  the kvs specified in the key-value map, but only if trackalert was
  started with the (undocumented) -v flag (for verbose). For example:
  
		debugLog("This will only log if trackalert is started with -v", { logging=1, foo=bar })

* LoginTuple - The only parameter to the report
  function is a LoginTuple table. This table contains the following
  fields:
  
* LoginTuple.remote - An IP address of type ComboAddress,
  representing the system performing login.

* LoginTuple.login - The username of the user performing the login.
  		
* LoginTuple.pwhash - A hashed representation of the users password
  		
* LoginTuple.success - Whether the user login was successful.

* LoginTuple.policy_reject - If the login was not successful only because of a
  policy-based reject from trackalert (i.e. the username and password were correct).

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
  device, which is parsed from the device_id string. The protocol
  string is used to determine how to parse device_id, so that MUST
  also be present. If device_attrs is already populated, it will be
  used as-is. For all protocols, the following keys are set
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
*/etc/wforce/trackalert.conf*

# SEE ALSO
trackalert(1)
