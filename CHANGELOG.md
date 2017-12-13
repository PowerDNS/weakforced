# Changelog
All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](http://keepachangelog.com/en/1.0.0/).

## [Unreleased]
### Added
- Add configuration setting "setNumWebHookConnsPerThread"

### Changed
- Refactor webhooks to use libcurl multi interface for performance and
deprecate per-webhook "num_conns" config


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
