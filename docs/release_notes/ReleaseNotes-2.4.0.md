# Release Notes for OX Abuse Shield 2.4.0

## New Features

* New wf_dump_entries tool to dump stats DBs to file
* Support for new "forwarding" type in replication messages
* Support for Prometheus via the new /metrics REST endpoint
* Add Date, Last-Modified and Cache-Control headers to all responses
* Session-ID now logged for allow/report commands
* Improved logging to show line numbers for Lua errors

## Bug Fixes/Changes
* Fix duplicate command stats under some circumstances

## New wf_dump_entries Tool

New tool to dump the contents of Stats DBs for debugging purposes.
`man wforce_dump_entries` for more information.

## Forwarding Type in Replication Messages

Replication messages now have a 'forwarding' flag, which is used to
indicate when a message has been forwarded. This can be used to
prevent forwarding loops.

## Support for Prometheus Metrics

Both the wforce and trackalert daemons support native Prometheus
metrics via the new /metrics REST API endpoint. This endpoint follows
the format described
[here](https://prometheus.io/docs/instrumenting/exposition_formats/).

The prometheus metrics deprecate the existing metrics functionality
including the following console commands:
* showPerfStats()
* showCommandStats()
* showCustomStats()

as well as the following REST API endpoints:
* /?command=stats

The prometheus metrics include metrics for many components that were
not previously instrumented, including:
* Redis statistics
* DNS Queries
* Whitelists and blacklists

## New HTTP Response Headers

All HTTP responses now include the following headers:
* Last-Modified
* Date
* Cache-Control

Last-Modified and Date headers will always reflect the current
date/time as seen by the wforce server.

## Session-ID Logging

The allow and report logs will now contain session_id information.

## Improved Logging for Lua Errors

The Lua wrapper code has been updated to provide better traceback
information, including line numbers, for Lua errors. This helps when
writing Lua policy that triggers a Lua exception.

## Fix Duplicate Command Stats

Under certain circumstances, relating to EOF handling when sockets are
closed, REST API command statistics would be double counted. This has
been fixed by refactoring the EOF handling code.
