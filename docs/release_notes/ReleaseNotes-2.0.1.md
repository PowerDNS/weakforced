# Release Notes for OX Abuse Shield 2.0.1

New Features/Bug Fixes
------------

* Fix issue where blacklisted is always true in getDBStats output
* Fix issue where threadnames were not displayed correctly
* Fix issue where siblings defined as tcp were connected to on startup
* Fix issue where non-UTF-8 login names would cause protobuf erros

Fix blacklisted always true in getDBStats output
--------------

Fix unitialized variable causing getDBStats command to always return
blacklisted: true.

Fix Threadnames Display Issue
---------------

Threadn support was implemented, but wasn't working due to compile
dependency issues. This is now fixed meaning that top will show thread
names when called with the -H option for example.

Fix Sibling TCP Connect Issue on Startup
----------------

Support for sibling using tcp was added in 2.0.0, however when
siblings are defined as TCP, wforce was attempting to connect to them
on startup. If several wforce servers were started at the same time,
this would cause a delay while each tries to connect to the other over
TCP but fails. This fix delays TCP connection until the first
replication attempt. Note that any static blacklist entries in the
config will cause a replication attempt on startup and this will
trigger the same startup delay behaviour. Thus it is not recommended to
create static blacklist entries in the config.

Fix non-UTF-8 Login Name Issue
----------------

Replication of data used the protobuf "string" type, which gets
validated on parsing to ensure it is a UTF-8 string. However, certain
fields such as login can contain non-UTF-8 characters, so this fix
changes to use the "bytes" type instead. This fix is backwards
compatible because string and bytes types are identical on the wire.
