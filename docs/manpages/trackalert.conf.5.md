% TRACKALERT.CONF(5)
% Dovecot Oy
% 2017

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
  from the console. See *trackalert(1)* for instructions on
  running a console client. For example:
  
		setKey("Ay9KXgU3g4ygK+qWT0Ut4gH8PPz02gbtPeXWPdjD0HE=")

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

* setNumReportThreads(\<num threads\>) - Set the number of threads in
  the pool used to run Lua report functions. Each thread uses a
  separate Lua Context, (see setNumLuaStates()). Defaults to 6 if not
  specified. For example:
  
		setNumReportThreads(6)

* setNumSchedulerThreads(\<num threads\>) - Set the number of threads
  in the pool used to run scheduled background Lua functions. Each
  thread uses a separate Lua Context, the number of which is set with
  setNumLuaStates(). Defaults to 4 if not specified. For example:
  
		setNumSiblingThreads(2)

* setNumWebHookThreads(\<num threads\>) - Set the number of threads in
  the pool used to send webhook events. Defaults to 4 if not
  specified. For example:

		setNumWebHookThreads(2)

* initGeoIPDB() - Initializes the country-level IPv4 and IPv6 GeoIP
  databases. If either of these databases is not installed, this
  command will fail and trackalert will not start. For example:
  
		initGeoIPDB()

* initGeoIPCityDB() - Initializes the city-level IPv4 and IPv6 GeoIP
  databases. If either of these databases is not installed, this
  command will fail and trackalert will not start. Ensure these
  databases have the right names if you're using the free/lite DBs -
  you may need to create symbolic links e.g. GeoIPCityv6.dat ->
  GeoLiteCityv6.dat. For example: 
  
		initGeoIPCityDB()

* initGeoIPISPDB() - Initializes the ISP-level IPv4 and IPv6 GeoIP
  databases. If either of these databases is not installed, this
  command will fail and trackalert will not start. For example:
  
		initGeoIPISPDB()


* setReport(\<report func\>) - Tell trackalert to use the specified
  Lua function for handling all "report" commands. For example:
  
		setReport(report)

* setBackground(\<name\>, \<lua function\> - The setBackground
  function registers a background function with a given name. The name
  is used when scheduling the function using the
  scheduleBackgroundFunc() function.

		setBackground("mybg", backgroundFunc)

# GENERAL FUNCTIONS

The following functions are available anywhere; either as part of the 
configuration or within the allow/report/reset functions:

* lookupCountry(\<ComboAddress\>) - Returns the two-character country
  code of the country that the IP address is located in. A
  ComboAddress object can be created with the newCA() function. For example:
  
		my_country = lookupCountry(my_ca)

* lookupISP(\<ComboAddress\>) - Returns the name of the ISP hosting
  the IP address. A ComboAddress object can be created with the
  newCA() function. For example:

		local my_isp = lookupCountry(newCA("128.243.16.21"))

* lookupCity(\<ComboAddress\>) - Returns a map containing information
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
		    -- do something special for MacOS
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

* scheduleBackgroundFunc(\<cron string\>, \<background function
  name\>) - Tells trackalert to run the specified function according
  to the given cron schedule (note the
  name given in setBackground() is used, *not* the actual function
  name). Note that cron ranges are not currently supported - if you
  want to schedule the same function to run for example on two
  different days of the week, then you would use two different calls
  to this function to achieve that. For example:

		scheduleBackgroundFunc("0 0 1 * *", "mybg")
		scheduleBackgroundFunc("0 0 6 * *", "mybg")

# FILES
*/etc/trackalert.conf*

# SEE ALSO
trackalert(1)
