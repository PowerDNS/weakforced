# Release Notes for OX Abuse Shield 2.0.0

New Features
------------

* Lookup individual GeoIP2 values (Unsigned Integer, String,
Double/Float and Boolean)
* Add session_id to wforce LoginTuple
* The Report API is now deployable via proper packaging, systemd and gunicorn
* Custom GET Endpoints supporting non-JSON return values
* New Kibana Reports and Dashboard to view effectiveness of wforce
policy
* New "type" field added to built-in webhooks
* Built-in whitelists added in addition to built-in blacklists
* New checkBlacklist and checkWhitelist functions in Lua
* Built-in black/whitelisting can be disabled and checked from Lua
instead
* Thread names support
* Built-In Blacklist and Whitelist return messages are configurable
* Support TCP Keepalive

Bug Fixes/Changes
-----------------
* Fix typo in wforce.conf.example blackistLogin->blacklistLogin
* Python3 support throughout wforce (regression tests and deployment)
* Uses "bytes" instead of "string" in replication, which avoids
protobuf errors for non-UTF-8 StatsDB keys or values.
* Remove ctpl from wforce
* Sibling threads now use explicit std::queue instead of ctpl
* Only connect to TCP siblings when necessary (not on startup) to
prevent delays on startup.
* Updated logstash.conf file for ELK integration


Lookup Individual GeoIP2 Values
----------

Previously the GeoIP2 support was limited to retrieving City and
County DB information, with a fixed set of fields returned. However
this meant that other DBs (e.g. ISPs, Anonymizers etc.) could not be
queried, as well as additional fields in City or Country DBs.

Now there are four new Lua functions to query arbitrary fields in the
Maxmind DBs:
* lookupStringValue
* lookupDoubleValue
* lookupUIntValue
* lookupBoolValue

For example:

    local citydb = getGeoIP2DB("City")
    local city_name = citydb:lookupStringValue(newCA(ip_address), {"city", "names", "en"})
    local city_lat = citydb:lookupDoubleValue(newCA(ip_address), {"location", "latitude"})
    local city_long = citydb:lookupDoubleValue(newCA(ip_address), {"location", "longitude"})
    local accuracy = citydb:lookupUIntValue(newCA(ip_address), {"location", "accuracy_radius"})
    local eu = citydb:lookupBoolValue(newCA(ip_address), {"country", "is_in_european_union"})

Add session_id to wforce LoginTuple
----------

It can be useful for wforce to recognise when multiple logins belong
to the same session. Therefore a new field "session_id" is added to
the LoginTuple table, which allows a session id to be passed to
wforce. This field is optional, so if empty it should be ignored. For
example:

    local session_id = lt.session_id

Deployable Report API
------------

Previously the Report API was supplied as an experimental feature,
which was supplied as a Flask application, but without support for running
in production, packaging etc.

Now the Report API is fully deployable; it is packaged in a new
package "wforce-report-api", and can be started/stopped via
systemd. For example:

    systemctl enable wforce-report-api
    systemctl start wforce-report-api

The Report API is still a Flash application, which runs under
gunicorn, and the configuration and deployment settings are
configurable.

Report API configuration is via:

    /etc/wforce-report-api/wforce-report-api-instance.conf

Gunicorn configuration is via:

    /etc/wforce-report-api/wforce-report-api-web.conf

It is recommended to run Gunicorn behind an nginx proxy in a
production deployment.

Custom GET Endpoints
-------

The existing Custom Endpoint functionality is based only on POST
commands, and requires return values to be json-encoded. This presents
problems with more limited clients such as firewalls or other network
equipment, which only support HTTP GET, and cannot parse json-encoded
return values.

The Custom GET endpoints solve this issue by allowing endpoints to be
created which are based on HTTP GET, and which return data using
the text/plain content type.

Custom GET endpoints do not take any parameters, so there is no way to
pass data to the endpoints.

For example the following function uses the new getIPBlacklist()
functionality to return a text version of the IP blacklist:

    function returnTextBlacklist()
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
        unblacklistIP(newCA("1.2.3.4"))
        return s
    end

    setCustomGetEndpoint("textBlacklist", returnTextBlacklist)

New Kibana Reports and Dashboard
----------

New Kibana Reports and Dashboard have been added to the
kibana_saved_objects.json file. These reports are based on "allow"
webhooks sent to elasticsearch.

New "type" field added to built-in webhooks
------

The built-in webhooks add a "type" field to the webhook json to make
it easier to search for specific webhook types in elasticsearch. The
type field can have the following values:
* wforce_report
* wforce_allow
* wforce_reset
* wforce_expireblwl
* wforce_addblwl
* wforce_delblwl

Built-in Whitelists
------

In addition to the built-in blacklists there are now built-in
whitelists. Entries can be added and deleted from the built-in
whitelists using either the REST API or using Lua commands.

For example using the REST API:

    curl -XPOST --data '{ "ip":"1.2.3.4" }' -u user:pass http://localhost:8084/?command=addWLEntry

For example using Lua:

    whitelistIP(lt.remote, 3600, "This is the reason why it is blacklisted")

New checkBlacklist and checkWhitelist functions in Lua
-------------

New functions to check the built-in black and whitelists are available
from Lua:
* checkBlacklistIP
* checkBlacklistLogin
* checkBlacklistIPLogin
* checkWhitelistIP
* checkWhitelistLogin
* checkWhitelistIPLogin

See the wforce.conf man page for more details.

Built-in Black/Whitelisting can be disabled
----------

The built-in black/whitelisting functionality can be disabled so that
the checks can instead be performed from Lua. This is achieved with
the following Lua commands:

    disableBuiltinBlacklists()
    disableBuiltinWhitelists()

Thread Names Support
--------

Every thread type in wforce is now named, so that top -H for example
will show individual thread names. This can be very useful for
diagnosing when particular threads are CPU-bound and could benefit
from increasing the size of the thread pool for example.

Built-In Blacklist return messages are configurable
---------------

The following new functions enable the return messages for built-in blacklists to be configured:
* setBlacklistIPRetMsg
* setBlacklistLoginRetMsg
* setBlacklistIPLoginRetMsg

See the wforce.conf man page for more details.

Support TCP Keepalive
----------

The wforce daemon previously did not enable TCP keepalive on 
accepted sockets. The TCP keepalive socketoption is now enabled for
all sockets.
