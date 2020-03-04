# Changelog
All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](http://keepachangelog.com/en/1.0.0/).

## [Unreleased]

## [2.2.2]

### Added
- Support building debian buster packages
- StatsDBs can now be sharded for better performance
- StatsDB expiry thread now runs more often by default
- StatsDB expiry thread sleep time is now configurable

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
