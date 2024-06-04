# Changelog
All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [1.4.0] - 2023-11-04
### Changed
* Add optional timeout argument to `open` and `open_simple`

## [1.3.1] - 2023-03-15
### Fixed
* Fix segfault in `search()` with multiple attrs

### Changed
* Improve example with `search()` in manual

## [1.3.0] - 2021-06-05
### Added
* a new method `open` which deprecates `initialize`

### Changed
* `bind_simple` returns the connection (instead of `true`)
* the Lua compatibility (5.1 to 5.4) is done without the compat-5.3 module

## [1.2.6] - 2021-03-17
### Added
* Lua 5.4 compatibility
* Updated documentation is available on <https://lualdap.github.io/lualdap/>

### Fixed
* Fix case in Makefile when install directory does not exist
* WinLDAP variant
* issue #10: space-separated list of hostnames doesn't work anymore

### Removed
* Dropped the claim that we support OpenLDAP older than 2.3, which was not true since 3597ac91334dbaa912db391f5e9fd7a15a643686

## [1.2.5] - 2019-01-06
### Changed
* `CPPFLAGS`, `CFLAGS` and `LDFLAGS` can now be overridden on the `make` command line.  It is not longer necessary to edit the `config` file.

### Removed
* We no longer export a `lualdap` global variable, in accordance with Lua 5.2 module rules (#8)
  - cf. https://www.lua.org/manual/5.2/manual.html#8.2
* Remove support for Lua 5.0 by including an external file outside the source directory
  - Support for Lua 5.1 and 5.2 continues through our inclusion of lua-compat-5.3 (see release notes for v1.2.4-rc1)

## [1.2.4] - 2019-01-02
### Added
* Build system additions to accomodate Debian

## [1.2.4-rc1] - 2018-12-22
### Added
* Lua 5.3 compatibility
  - Backwards compatibility using Kepler Project's [lua-compat-5.3](https://github.com/keplerproject/lua-compat-5.3/)
* Support specifying a URI in hostname argument to `open_simple()`

### Changed
* Switch to [busted](http://olivinelabs.com/busted/) unit testing framework
* Automate building and running unit tests using [CircleCI](http://circleci.com/)
  - Tests run against [OpenShift's OpenLDAP 2.4.41](https://hub.docker.com/r/openshift/openldap-2441-centos7/) ([source](https://github.com/openshift/openldap/))
* Keep track of unit test coverage using [Codecov](http://codecov.io/)

### Fixed
* C89 compatibility
* Fix two credentials-related segfaults in `open_simple()`

## [1.2.3] - 2015-04-09
### Changed
* documentation

## [1.2.2] - 2014-07-26
### Changed
* `lualdap_bind_simple()` with OpenLDAP 2.3 API

## [1.2.1] - 2014-06-29
### Added
* from prosody.im, new function `initialize` and new method `bind_simple`

## [1.2.0] - 2014-02-21
### Added
* Lua 5.2 compatibility
* OpenLDAP 2.3 compatibility
* rockspec

## 1.1.0 - 2007-12-14
### Added
* support ADSI (WinLDAP) (Thanks to Mark Edgar)

## 1.0.1 - 2006-04-04

## 1.0 - 2005-06-10

## 1.0-alpha - 2003-12-10

[Unreleased]: https://github.com/lualdap/lualdap/compare/v1.4.0...HEAD
[1.4.0]: https://github.com/lualdap/lualdap/compare/v1.3.1...v1.4.0
[1.3.1]: https://github.com/lualdap/lualdap/compare/v1.3.0...v1.3.1
[1.3.0]: https://github.com/lualdap/lualdap/compare/v1.2.6...v1.3.0
[1.2.6]: https://github.com/lualdap/lualdap/compare/v1.2.5...v1.2.6
[1.2.5]: https://github.com/lualdap/lualdap/compare/v1.2.4...v1.2.5
[1.2.4]: https://github.com/lualdap/lualdap/compare/v1.2.4-rc1...v1.2.4
[1.2.4-rc1]: https://github.com/lualdap/lualdap/compare/v1.2.3...v1.2.4-rc1
[1.2.3]: https://github.com/lualdap/lualdap/compare/v1.2.2...v1.2.3
[1.2.2]: https://github.com/lualdap/lualdap/compare/v1.2.1...v1.2.2
[1.2.1]: https://github.com/lualdap/lualdap/compare/v1.2.0...v1.2.1
[1.2.0]: https://github.com/lualdap/lualdap/compare/v1_1_0...v1.2.0

