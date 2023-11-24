Weakforced
----------

The goal of 'wforce' is to detect brute forcing of passwords across many
servers, services and instances.  In order to support the real world, brute
force detection policy can be tailored to deal with "bulk, but legitimate"
users of your service, as well as botnet-wide slowscans of passwords.

The aim is to support the largest of installations, providing services to
hundreds of millions of users.  The current version of weakforced is not
quite there yet, although it certainly scales to support up to ten
million users, if not more. The limiting factor is number of logins
per second at peak.

Wforce is a project by Dovecot, PowerDNS and Open-Xchange. For historical
reasons, it lives in the PowerDNS github tree. If you have any questions, email
neil.cook@open-xchange.com.

For detailed technical documentation, please go to [https://powerdns.github.io/weakforced/](https://powerdns.github.io/weakforced/).

Here is how it works:
 * Report successful logins via JSON http-api
 * Report unsuccessful logins via JSON http-api
 * Query if a login should be allowed to proceed, should be delayed, or ignored via http-api
 * API for querying the status of logins, IP addresses etc.
 * Runtime console for server introspection

wforce is aimed to receive message from services like:

 * IMAP
 * POP3
 * Webmail logins
 * FTP logins
 * Authenticated SMTP
 * Self-service logins
 * Password recovery services

By gathering failed and successful login attempts from as many services as
possible, brute forcing attacks as well as other suspicious behaviour
can be detected and prevented more effectively.

Inspiration:
http://www.techspot.com/news/58199-developer-reported-icloud-brute-force-password-hack-to-apple-nearly-six-month-ago.html

Installing
----------

Docker:

There is a docker image hosted on docker hub, see [https://powerdns.github.io/weakforced/](https://powerdns.github.io/weakforced/) for more details.

From GitHub:

The easy way:

```
$ git clone https://github.com/PowerDNS/weakforced.git
$ cd weakforced
$ git submodule init
$ git submodule update
$ builder/build.sh debian-bullseye | debian-stretch | centos-7 | ol-8 | amazon-2
```
This will build packages for the appropriate OS. You will need docker and docker-compose for the builder to work.

The hard way:

```
$ git clone https://github.com/PowerDNS/weakforced.git
$ cd weakforced
$ autoreconf -i
$ ./configure
$ make
```

This requires recent versions of libtool, automake and autoconf to be
installed.

It also requires:
 * A compiler supporting C++ 17
 * Lua 5.1+ development libraries (or LuaJIT if you configure --with-luajit)
 * Boost 1.61+
 * Protobuf compiler and protobuf development libraries
 * Getdns development libraries (if you want to use the DNS lookup functionality)
 * libsodium
 * python + virtualenv for regression testing
 * libgeoip-dev for GeoIP support
 * libsystemd-dev for systemd support
 * pandoc for building the manpages
 * libcurl-dev (OpenSSL version)
 * libhiredis-dev
 * libssl-dev
 * libprometheus-cpp (https://github.com/jupp0r/prometheus-cpp)
 * libmaxminddb-dev
 * libyaml-cpp-dev
 * libdrogon (https://github.com/drogonframework/drogon) - Used for the HTTP server
 * libjsoncpp-dev
 * libuuid-dev
 * libz-dev
 * docker for regression testing
 * python3 rather than python2
 * python-bottle for regression testing of webhooks

To build on OS X, `brew install readline` and use
`./configure PKG_CONFIG_PATH=<path to your openssl installation> LDFLAGS=-L/usr/local/opt/readline/lib CPPFLAGS=-I/usr/local/opt/readline/include`

Add --with-luajit to the end of the configure line if you want to use LuaJIT.

Policies
--------

There is a sensible, if very simple, default policy in wforce.conf (running without
this means *no* policy), and extensive support for crafting your own policies using
the insanely great Lua scripting language.

Note that although there is
a single Lua configuration file, the canonicalize, reset, report and allow functions run in
different lua states from the rest of the configuration. This mostly
"just works", but may lead to unexpectd behaviour such as running Lua
commands at the server Lua prompt, and getting multiple answers
(because Lua commands are passed to all Lua states).

Sample:

```lua
-- set up the things we want to track
field_map = {}
-- use hyperloglog to track cardinality of (failed) password attempts
field_map["diffFailedPasswords"] = "hll"
-- track those things over 6x10 minute windows
newStringStatsDB("OneHourDB", 600, 6, field_map)

-- this function counts interesting things when "report" is invoked
function twreport(lt)
	sdb = getStringStatsDB("OneHourDB")
	if (not lt.success)
	then
	   sdb:twAdd(lt.remote, "diffFailedPasswords", lt.pwhash)
	   addrlogin = lt.remote:tostring() .. lt.login
	   sdb:twAdd(addrlogin, "diffFailedPasswords", lt.pwhash)
	end
end

function allow(lt)
	sdb = getStringStatsDB("OneHourDB")
	if(sdb:twGet(lt.remote, "diffFailedPasswords") > 50)
	then
		return -1, "", "", {} -- BLOCK!
	end
	// concatenate the IP address and login string
	addrlogin = lt.remote:tostring() .. lt.login	
	if(sdb:twGet(addrlogin, "diffFailedPasswords") > 3)
	then
		return 3, "tarpitted", "diffFailedPasswords", {} -- must wait for 3 seconds
	end

	return 0, "", "", {} -- OK!
end
```

Many more metrics are available to base decisions on. Some example
code is in [wforce.conf](wforce/wforce.conf), and more extensive examples are
in [wforce.conf.example](wforce/wforce.conf.example). For full
[documentation](docs/manpages), use "man wforce.conf".

To report (if you configured with 'webserver("127.0.0.1:8084", "secret")'):

```bash
$ for a in {1..101}
  do
    curl -X POST -H "Content-Type: application/json" --data '{"login":"ahu", "remote": "127.0.0.1", "pwhash":"1234'$a'", "success":"false"}' \
    http://127.0.0.1:8084/?command=report -u wforce:secret
  done
```

This reports 101 failed logins for one user, but with different password hashes.

Now to look up if we're still allowed in:

```bash
$ curl -X POST -H "Content-Type: application/json" --data '{"login":"ahu", "remote": "127.0.0.1", "pwhash":"1234"}' \
  http://127.0.0.1:8084/?command=allow -u wforce:super
{"status": -1, "msg": "diffFailedPasswords"}
```

It appears we are not!

You can also provide additional information for use by weakforce using
the optional "attrs" object. An example:

```bash
$ curl -X POST -H "Content-Type: application/json" --data '{"login":"ahu", "remote": "127.0.0.1",
"pwhash":"1234", "attrs":{"attr1":"val1", "attr2":"val2"}}' \
  http://127.0.0.1:8084/?command=allow -u wforce:super
{"status": 0, "msg": ""}
```

An example using the optional attrs object using multi-valued
attributes:

```bash
$ curl -X POST -H "Content-Type: application/json" --data '{"login":"ahu", "remote": "127.0.0.1",
"pwhash":"1234", "attrs":{"attr1":"val1", "attr2":["val2","val3"]}}' \
  http://127.0.0.1:8084/?command=allow -u wforce:super
{"status": 0, "msg": ""}
```

There is also a command to reset the stats for a given login and/or IP
Address, using the 'reset' command, the logic for which is also
implemented in Lua. The default configuration for reset is as follows:

```lua
function reset(type, login, ip)
	 sdb = getStringStatsDB("OneHourDB")
	 if (string.find(type, "ip"))
	 then
		sdb:twReset(ip)
	 end
	 if (string.find(type, "login"))
	 then
		sdb:twReset(login)
	 end
	 if (string.find(type, "ip") and string.find(type, "login"))
	 then
		iplogin = ip:tostring() .. login
		sdb:twReset(iplogin)
	 end
	 return true
end
```

To test it out, try the following to reset the login 'ahu':

```bash
$ curl -X POST -H "Content-Type: application/json" --data '{"login":"ahu"}'\
  http://127.0.0.1:8084/?command=reset -u wforce:super
{"status": "ok"}
```

You can reset IP addresses also:

```bash
$ curl -X POST -H "Content-Type: application/json" --data '{"ip":"128.243.21.16"}'\
  http://127.0.0.1:8084/?command=reset -u wforce:super
{"status": "ok"}
```

Or both in the same command (this helps if you are tracking stats using compound keys
combining both IP address and login):

```bash
$ curl -X POST -H "Content-Type: application/json" --data '{"login":"ahu", "ip":"FE80::0202:B3FF:FE1E:8329"}'\
  http://127.0.0.1:8084/?command=reset -u wforce:super
{"status": "ok"}
```

Finally there is a "ping" command, to check the server is up and
answering requests:

```bash
$ curl -X GET http://127.0.0.1:8084/?command=ping -u wforce:super
{"status": "ok"}
```

Console
-------
Available over TCP/IP, like this:
```lua
setKey("Ay9KXgU3g4ygK+qWT0Ut4gH8PPz02gbtPeXWPdjD0HE=")
controlSocket("0.0.0.0:4004")
```

Launch wforce as a daemon (`wforce --daemon`), to connect, run `wforce -c`.
Comes with autocomplete and command history. If you put an actual IP address
in place of 0.0.0.0, you can use the same config to listen and connect
remotely.

To get some stats, try:
```lua
> stats()
40 reports, 8 allow-queries, 40 entries in database
```

The [wforce manpage](docs/manpages/wforce.1.md) describes the
command-line options and all the possible console commands in more detail.

Spec
----
Wforce accepts reports with 4 mandatory fields plus multiple optional
fields.

Mandatory:
 * login (string): the user name or number or whatever
 * remote (ip address): the address the user arrived on
 * pwhash (string): a highly truncated hash of the password used
 * success (boolean): was the login a success or not?

Optional:
 * policy_reject (boolean) -  If the login was not successful only because of a policy-based reject from wforce (i.e. the username and password were correct).
 * attrs (json object): additional information about the login. For
 example, attributes from a user database.
 * device_id (string) - A string that represents the device that the user
   logged in from. For HTTP this would typically be the User-Agent
   string, and for IMAP it would be the IMAP client ID command string.
 * protocol (string) - A string representing the protocol used to login,
 e.g. "http", "imap", "pop3".
 * tls (boolean) - Whether or not the login was secured with TLS.

The entire HTTP API is [documented](docs/swagger/wforce_api.7.yml)
using the excellent OpenAPI (swagger) specification. 

The pwhash field deserves some clarification. In order to
distinguish actual brute forcing of a password, and repeated incorrect but
identical login attempts, we need some marker that tells us if passwords are
different.

Naively, we could hash the password, but this would spread knowledge of
secret credentials beyond where it should reasonably be. Even if we salt and
iterate the hash, or use a specific 'slow' hash, we're still spreading
knowledge.

However, if we take any kind of hash and truncate it severely, for example
to 12 bits, the hash tells us very little about the password itself - since
one in 4096 random strings will match it anyhow. But for detecting multiple
identical logins, it is good enough.

For additional security, hash the login name together with the password - this
prevents detecting different logins that might have the same password.

NOTE: wforce does not require any specific kind of hashing scheme, but it
is important that all services reporting successful/failed logins use the
same scheme!

When in doubt, try:

```SQL
TRUNCATE(SHA256(SECRET + LOGIN + '\x00' + PASSWORD), 12)
```

Which denotes to take the first 12 bits of the hash of the concatenation of
a secret, the login, a 0 byte and the password.  Prepend 4 0 bits to get
something that can be expressed as two bytes.

API Calls
---------
We can call 'report', and 'allow' commands. The optional 'attrs' field
enables the client to send additional data to weakforced.

To report, POST to /?command=report a JSON object with fields from the
LoginTuple as described above.

To request if a login should be allowed, POST to /?command=allow, again with
the LoginTuple. The result is a JSON object with a "status" field. If this is -1, do
not perform login validation (i.e. provide no clue to the client if the password
was correct or not, or even if the account exists).

If 0, allow login validation to proceed. If a positive number, sleep this
many seconds until allowing login validation to proceed.

Custom API Endpoints
--------------------

You can create custom API commands (REST Endpoints) using the
following configuration:

```lua
setCustomEndpoint("custom", customfunc)
```

which will create a new API command "custom", which calls the Lua
function "customfunc" whenever that command is invoked. Parameters to
custom commands are always in the same form, which is key-value pairs
wrapped in an 'attrs' object. For example, the following parameters
sents as json in the message body would be valid:

```json
{ "attrs" : { "key" : "value" }}
```

Custom functions return values are also key-value pairs, this time
wrapped in an 'r_attrs' object, along with a boolean success field,
for example:

```json
{ "r_attrs" : { "key" : "value" }, "success" : true}
```

An example configuration for a custom API endpoint would look like:

```lua
function custom(args)
	for k,v in pairs(args.attrs) do
		infoLog("custom func argument attrs", { key=k, value=v });
	end
	-- return consists of a boolean, followed by { key-value pairs }
	return true, { key=value }
end
setCustomEndpoint("custom", custom)
```

An example curl command would be:

```lua
% curl -v -X POST -H "Content-Type: application/json" --data
  '{"attrs":{"login1":"ahu", "remote": "127.0.0.1",  "pwhash":"1234"}}'
  http://127.0.0.1:8084/?command=custom -u wforce:super
{"r_attrs": {}, "success": true}
```

WebHooks
---------
It is possible to configure webhooks, which get called whenever
specific events occur. To do this, use the "addWebHook" configuration
command. For example:

```lua
config_keys={}
config_keys["url"] = "http://webhooks.example.com:8080/webhook/"
config_keys["secret"] = "verysecretcode"
events = { "report", "allow" }
addWebHook(events, config_keys)
```

The above will call the webhook at the specified url, for every report
and allow command received, with the body of the POST containing the
original json data sent to wforce. For more information use ["man
wforce.conf"](docs/manpages/wforce.conf.5.md) and
["man wforce_webhook"](docs/manpages/wforce_webhook.5.md).

Custom WebHooks
----------------

Custom webhooks can also be defined, which are not invoked based on
specific events, but instead from Lua. Configuration is similar to
normal webhooks:

```lua
config_keys={}
config_keys["url"] = "http://webhooks.example.com:8080/webhook/"
config_keys["secret"] = "verysecretcode"
config_keys["content-type"] = "application/json"
addCustomWebHook("mycustomhook", config_keys)
```

However, the webhook will only be invoked via the Lua
"runCustomWebHook" command, for example:

```lua
runCustomWebHook(mycustomhook", "{ \"foo\":\"bar\" }")
```

The above command will invoke the custom webhook "mycustomhook" with
the data contained in the second argument, which is simply a Lua
string. No parsing of the data is performed, however the Content-Type
of the webhook, which defaults to application/json can be customized
as shown above.

Blacklists
----------

Blacklisting capability is provided via either REST endpoints or Lua
commands, to add/delete IP addresses, logins or IP:login tuples from
the Blacklist. Blacklist information can be replicated (see below),
and also optionally persisted in a Redis DB. Use "man wforce.conf" to
learn more about the blacklist commands.

Load balancing: siblings
------------------------
For high-availability or performance reasons it may be desireable to run
multiple instances of wforce. To present a unified view of status however,
these instances then need to share data. To do so, wforce
implements a simple knowledge-sharing system.

The original version of wforce simply broadcast all received report
tuples (best effort, UDP) to all siblings. However the latest version
only broadcasts incremental changes to the underlying state databases,
namely the stats dbs and the blacklist.

The sibling list is parsed such that we don't broadcast messages to ourselves
accidentally, and can thus be identical across all servers.

Even if you configure siblings, stats db data is not replicated by default. To do
this, use the "twEnableReplication()" command on each
stats db for which you wish to enable replication. Blacklist
information is automatically replicated if you have configured siblings.

To define siblings, use:

```lua
setKey("Ay9KXgU3g4ygK+qWT0Ut4gH8PPz02gbtPeXWPdjD0HE=")
addSibling("192.168.1.79")
addSibling("192.168.1.30")
addSibling("192.168.1.54")
siblingListener("0.0.0.0")
```

The first line sets the authentication and encryption key for our sibling
communications. To make your own key (recommended), run `makeKey()` on the
console and paste the output in all your configuration files.

This last line configures that we also listen to our other siblings (which
is nice).  The default port is 4001, the protocol is UDP.

To view sibling stats:

```lua
> siblings()
Address                             Sucesses  Failures     Note
192.168.1.79:4001                   18        7
192.168.1.30:4001                   25        0
192.168.1.54:4001                   0         0            Self
```

With this setup, several wforces are all kept in sync, and can be load
balanced behind (for example) haproxy, which incidentally can also offer SSL.

GeoIP2 Support
-------------

GeoIP support is provided using the GeoIP2 Maxmind APIs DBs
(i.e. DBs ending in .mmdb). This is the preferred integration to use,
as support for GeoIP Legacy DBs will be discontinued by Maxmind
in 2019.

GeoIP2 DBs are represented by a Lua object that is created with the
following call:

```lua
newGeoIP2DB("Name", "/path/to/file.mmdb")
```

The Lua object is retrieved with the following call:

```lua
local mygeodb = getGeoIP2DB("Name")
```

You can then lookup information using the following calls:

* lookupCountry() - Returns the 2 letter country code associated with
  the IP address
* lookupISP() - Returns the name of the ISP associated with the IP
  address (requires the Maxmind ISP DB, which is only available on
  subscription)
* lookupCity - Rather than only returning a city name, this call
returns a Lua table which includes the following information:
    * country_code
    * country_name
    * region
    * city
    * postal_code
    * continent_code
    * latitude
    * longitude

For example:

```lua
local geoip_data = mygeodp:lookupCity(newCA("128.243.21.16"))
print(geoip_data.city)
print(geoip_data.longitude)
print(geoip_data.latitude)
```

Legacy GeoIP Support
-------------

Support for legacy GeoIP databases (i.e. ending in .dat) is
deprecated, since Maxmind will be discontinuing support for them
in 2019.

Three types of GeoIP lookup are supported:

* Country lookups - Initialized with initGeoIPDB() and looked up
    with lookupCountry()
* ISP Lookups - Initialized with initGeoIPISPDB() and looked up with
  lookupISP()
* City Lookup - Initialized with initGeoIPCityDB() and looked up
  with lookupCity()

The Country and ISP lookups return a string, while lookupCity()
returns a Lua map consisting of the following keys:
* country_code
* country_name
* region
* city
* postal_code
* continent_code
* latitude
* longitude

For example:
```lua
local geoip_data = lookupCity(newCA("128.243.21.16"))
print(geoip_data.city)
```

When a DB is initialized, wforce attempts to open both v4 and v6
versions of the database. If either is not found an error is thrown,
so make sure both ipv4 and v6 versions of each DB are
installed.

Additionally, when using the free/lite versions of the
databases, you may see errors such as "initGeoIPCityDB(): Error
initialising GeoIP (No geoip v6 city db available)". This is usually
because the filenames for the "lite" DBs are not the same as the
expected filenames for the full DBs, specifically all files must
start with GeoIP rather than GeoLite. Creating symbolic links to the
expected filenames will fix this problem, for example:

```
ln -s GeoLiteCityv6.dat GeoIPCityv6.dat
```
