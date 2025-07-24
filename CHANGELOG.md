# Changelog
All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](http://keepachangelog.com/en/1.0.0/).

## [Unreleased]

### Added
- Support new `fail_type` parameter for determining why a login failed
- New livez and readyz endpoints (unauthenticated) for k8s environments
- Support JA3 allow/block lists in API/Lua
- Allow ja3 to be passed to the reset API command
- Add support for building amazon-2023 packages
- Use asciidoctor to build documentation not pandoc

### Removed
- Removed support for Enterprise Linux 7 and Amazon 2

## [2.12.1]

### Added
- Set default map size for sharded Stats DBs based on the number of shards
- Change debian postinst scripts to modify 'setKey' and 'webpwd' parameter only if no weakforce is already installed
- Add tests for crypto code

## [2.12.0]

### Added
- Now builds a separate luajit package, based on the openresty luajit fork. This is to address some issues found with stock luajit. The package also includes some lua modules that wforce typically makes use of.

### Changed
- Build the wforce-minimal image for both arm64 and amd64, and add provenance.
- Fix centos-7/el-7 builds to still work after centos-7 went EOL
- Add support for debian-bookworm, remove support for debian-buster

## [2.10.2]

### Changed
- Fixed LuaState selection algorithm to use a free pool, which should lead to faster/more 
  consistent selection of lua states by threads.

### Added
- CI now builds an additional image 'powerdns/wforce-minimal' using alpine for more secure and much smaller image

## [2.10.1]

### Changed
- Fixed bug in GeoIP2 lookups where return values were not populated

## [2.10.0]

### Added
- Add Enterprise Linux 9 Build Target
- Option to use OpenSSL instead of Libsodium for encryption

### Removed
- Remove Legacy GeoIP from Packages and Dockerfiles/Images
- Remove the report_api from weakforced entirely

### Changed
- Move to pytest instead of nose for regression tests

## [2.8.0]

### Added 
- Support ELK 7.x Stack 
- Support Date Expansion in WebHook URLs
- Enable IP and Login substitution in blocklist return messages
- Add config option to disable password for /metrics endpoint
- Support redis usernames and passwords for redis authentication
- Support hostnames for redis configuration in addition to IP addresses

## Changed

- Fix an issue where IPv6 ComboAddress returned zero port number (which caused v6 HTTP listen addresses to not work)
- Set V6ONLY socket option to stop v6 sockets from managing v4 addresses for replication
- Return the IP address of the client in JSON of ACL denied response

## [2.6.1]
- Fix issue where wforce was complaining about not being able to create tmp file on startup
- Fix timing issue whereby the webserver was not started before syncDB leading to syncDone failures
- Use debian bullseye-slim in wforce docker image
- Fix issue in wforce docker image where the default config file was overridden with a volume mount but not used

## [2.6.0]
- Support HTTPS in webserver using libdrogon
- Add support for configuring TLS behaviour of outbound HTTPS connections
- Remove support for building debian stretch packages
- Change behaviour of default ACLs such that they are overridden by setACL()
- Build for debian bullseye

## [2.4.1]

### Added
- New API and Lua commands to dynamically manage siblings
- Siblings can now have unique encryption keys
- Amazon Linux packaging target for pdns-builder

### Changed
- Fix issue where replication length bytes can be truncated causing syncDB problems

## [2.4.0]

### Added
- New wf_dump_entries tool to dump stats DBs to file
- Support for new "forwarding" type in replication messages
- Support for Prometheus via the new /metrics REST endpoint
- Enable TCP keepalive for redis connections
- Configurable timeout for R/W on redis connections

### Changed
- Fix duplicate command stats under some circumstances

## [2.2.2]

### Added
- Support building debian buster packages
- StatsDBs can now be sharded for better performance
- StatsDB expiry thread now runs more often by default
- StatsDB expiry thread sleep time is now configurable
- Support for Kafka REST Proxy in webhooks

### Changed
- Fix control socket leak when client closes connection immediately

## [2.2.1]

### Changed
- Fix wforce crash in Sibling send thread triggered by syncDB operation 

## [2.2.0]

### Added
- Lookup individual GeoIP2 values (Unsigned Integer, String,
Double/Float and Boolean)
- Add session_id to wforce LoginTuple
- The report_api is now deployable via proper packaging, systemd and gunicorn
- Custom GET Endpoints supporting non-JSON return values
- New Kibana Reports and Dashboard to view effectiveness of wforce
policy
- New "type" field added to built-in webhooks
- Built-in whitelists added in addition to built-in blacklists
- New checkXXXBlacklist (and Whitelist) function in Lua
- Built-in black/whitelisting can be disabled and checked from Lua
instead
- Thread names support
- Blacklisting return messages are configurable
- Add support for TCP keepalive
- New functions to retrieve the custom return message for blacklisted IPs, Logins and IP/Logins.

### Changed
- Fix typo in wforce.conf.example blackistLogin->blacklistLogin
- Python3 support throughout wforce (regression tests and deployment)
- Uses "bytes" instead of "string" in replication, which avoids
protobuf errors for non-UTF-8 StatsDB keys or values.
- Remove ctpl from wforce
- Sibling threads now use explicit std::queue instead of ctpl
- Perform replication to siblings asynchronously instead of synchronously,
  preventing delays at startup and in REST API functions which triggered
  replication.

## [2.0.0]

### Added
- Add configuration setting "setNumWebHookConnsPerThread"
- Add support for querying replication status in showStringStatsDB()
- Add sibling received success/fail stats to sibling() command
- New custom stats framework
- New stats for all commands, including custom commands
- GeoIP2 support (MMDB-style DBs)
- New resetField() function for statsDBs
- Support for building packages using pdnsbuilder
- Configurable accuracy for HLL and CountMin types
- DB Synchronization for newly started wforce instances
- Add configuration settings "addSyncHost" and "setMinSyncHostUptime"
- Add configuration setting "setWebHookTimeoutSecs"
- Support for replication over TCP
- Customizable log facility via a command line option
- Example logstash config and elasticsearch template in docs

### Deprecated
- GeoIP Legacy support

### Changed
- Refactor webhooks to use libcurl multi interface for performance and
deprecate per-webhook "num_conns" config
- Change log level of informational messages from warn to notice/info

## [1.4.3]

### Fixed
- Fix broken setVerboseAllowLog() function

## [1.4.2]

### Fixed
- Fix memory leak in statsDBs

## [1.4.1]

### Fixed
- Fix issue where mapped v4 addresses in v6 were not handled correctly

## [1.4.0] - 2017-10-04
### Added
- Support for additional GeoIP DBs: City and ISP
- Support for reloading GeoIP databases
- Blacklists now support IP netmasks as well as individual IP addresses
- Configurable timeout for Redis connections
- New Lua logging function: debugLog
- Webhook support for Basic Authentication
- Support for parsing device_id from http, imap and OX mobile clients
- New tls parameter to report/allow commands
- New console commands - showPerfStats() and reloadGeoIPDBs()

### Changed
- Wforce daemon changes working directory to config file directory
- Use Content-Length instead of chunked encoding for Webhooks

### Removed

### Deprecated

### Fixed
- Fixed a two bugs in dependent getdns library that could cause crashes

### Security
