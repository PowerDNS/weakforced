# Release Notes for OX Abuse Shield 2.8.0

<!-- {% raw %} -->

## New Features

* Support ELK 7.x Stack
* Support Date Expansion in WebHook URLs
* Enable IP and Login substitution in blocklist return messages
* Add config option to disable password for /metrics endpoint
* Support redis usernames and passwords for redis authentication
* Support hostnames for redis configuration in addition to IP addresses

## Bug Fixes/Changes

* Fix an issue where IPv6 ComboAddress returned zero port number (which caused v6 HTTP listen addresses to not work)
* Set V6ONLY socket option to stop v6 sockets from managing v4 addresses for replication
* Return the IP address of the client in JSON of ACL denied response

## Support ELK 7.x Stack

Support Elasticsearch, Logstash and Kibana 7.x stack:
* Continuous Integration now tests against ELK 7.x
* Logstash Templates now work with 7.x
* Kibana Dashboards are now in ndjson format

## Support Date Expansion in WebHook URLs

WebHook URLs can be specified with fields representing years, months and days that are expanded
at runtime, for example:
config_key["url"] = "https://example.com/foo/index-%{YYYY}-${MM}-{%dd}"

See the wforce_webhook man page for more details.

## Enable IP and Login Substitution in blocklist return messages

For example:
setBlacklistIPRetMsg("Go away your IP {ip} is blacklisted")
setBlacklistLoginRetMsg("Go away your login {login} is blacklisted")

See the wforce.conf man page for more details.

## Add config option to disable password for /metrics endpoint

Adding the following to wforce.conf or trackalert.conf:

setMetricsNoPassword()

will disable the password for the metrics endpoint.

See wforce.conf and trackalert.conf manpages for more details.

## Support redis usernames and passwords for redis authentication

Redis authentication is supported with the following configuration in wforce.conf:

blacklistRedisUsername()
blacklistRedisPassword()
whitelistRedisUsername()
whitelistRedisPassword()

The username is optional, depending on whether a username is set in redis.

See wforce.conf manpage for more details.

## Support hostnames for redis configuration in addition to IP addresses

The blacklistPersistDB() and whitelistPersistDB() configuration commands now accept
hostnames as well as IP addresses.

<!-- {% endraw %} -->