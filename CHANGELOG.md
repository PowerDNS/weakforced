# Changelog
All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](http://keepachangelog.com/en/1.0.0/).

## [Unreleased]

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

### Deprecated
- GeoIP Legacy support

### Changed
- Refactor webhooks to use libcurl multi interface for performance and
deprecate per-webhook "num_conns" config

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
