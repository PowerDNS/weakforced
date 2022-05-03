# Release Notes for OX Abuse Shield 2.6.2

## Bug Fixes/Changes

* Better error checking in blacklist loading to prevent deadlock
* Fix missing stub for setBlacklistIPRetMsg() Lua function
* Fix trackalert crash when schedules are used before global Lua state is initialised
* Return 401 with appropriate JSON instead of 404 when webserver ACL is used
* New --loglevel flag to control the log level of stdout logging

## Better error checking in blacklist loading to prevent deadlock

Under certain conditions, i.e. when Redis was available but non-responsive, the blacklist
loading function would not return, causing deadlock. This has been fixed.

## Fix missing stub for setBlacklistIPRetMsg() Lua function

The setBlacklistIPRetMsg() Lua function was missing a stub, which meant that it could not
be used. This has now been corrected.

## Fix trackalert crash when schedules are used before global Lua state is initialised

Fixed an issue where trackalert would crash when a schedule was created which ran immediately, before the global Lua
state was initialised.

## Return 401 with appropriate JSON instead of 404 when webserver ACL is used

Fixed an issue where the webserver ACL was causing 404 errors instead of 401 errors. Now
a 401 and an appropriate JSON message are returned.

## New --loglevel flag to control the log level of stdout logging

Previously there was no way to control the loglevel of the stdout logging, which meant that even
debug logging would be logged. Now there is a -l or --loglevel flag, which takes the value 0-7
(matching the syslog levels), and which defaults to 6 (infolog). This fix also applies to the
built-in webserver, which only logs to stdout, and which previously only logged errors, but which now 
obeys this flag.