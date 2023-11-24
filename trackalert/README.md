Trackalert
------
Trackalert is designed to be an optional service to complement
wforce. Whereas wforce provides a toolkit to combat  abuse of
logins such as password brute forcing in realtime, trackalert is
designed to look at abuse asynchronously, using long-term report data
stored in an external DB such as elasticsearch, and to send alerts on
potential login abuse.

Rationale
------

Trackalert is much simpler than wforce - the REST API provides access
to a single Lua hook "report", which is designed to ingest report
data, typically received from wforce via a webhook. Various functions
from wforce are available to the report function such as GeoIP
lookups and the ability to send custom Webhooks, however the report
function is mainly designed to allow the current login data received
in the report to be compared to aggregated data found in a DB such as
elasticsearch.
The [elasticqueries directory](elasticqueries) contains sample
elasticsearch queries that could be used to achieve this. Because
trackalert is decoupled from the login flow, the action that can be
taken if a report is deemed to be suspicious is not "block", rather it
is intended that the report function is used to send alerts to users
via email or other methods, to alert them about potentially suspicious
logins. It might be asked, "Why not implement this in wforce?",
however if you consider that wforce is designed to run as fast as
possible, without delaying logins, combined with the fact that
querying long-term datasets can be quite slow, it makes sense to
create a separate daemon to enable this behaviour.

Additionally trackalert allows for scheduled "background" functions to
be run periodically. Unlike the report function, which is intended to
be used to find out if a particular login was suspicious, the
background functions are intended to be used to find "whole system"
abuse. For example, by searching the login-term report DB to discover
IPs which are consistently generating unsuccessful login attempts, or
to discover users whose accounts are being attacker or are maybe
already compromised. Again the action here may be to send an email or
a webhook, however this time the target would be an administrator or
operations team. Theoretically trackalert background functions could
call the wforce API and blacklist individual IPs or users, thus
leading to realtime blocking.

In summary, trackalert has very different goals to wforce, and
attempts to provide a simple framework to fulfil them:

 * The ability to create Lua policy that responds to individual login
   reports and takes appropriate action. It is suggested that policies
   query a long-term DB such as elasticsearch for previously
   stored reports, and then alert based on comparing the long-term
   data with the current login information. Currently no default
   policy is provided to support that behaviour.

 * The ability to run scheduled (background) Lua functions that can be
   used to creates policies such as looking for long-term abuse trends
   of IP addresses and users, and alert based on that data. Again, no
   default policy is provided to implement that behaviour.

Console
-----

The console is very similar to wforce. Read about it in the
[trackalert documentation](../docs/manpages/trackalert.1.md), or "man trackalert".

REST API
---

The REST API is very similar to wforce, although it consists of just
two commands: "report" (using POST) and "stats" (using GET). Read more
using the [trackalert swagger documentation](../docs/swagger/trackalert_api.7.yml).

Configuration
----------

Configuration is very similar to wforce, although there are less
configuration options and less functions available via Lua. Read about
configuration in the
[trackalert configuration documentation](../docs/manpages/trackalert.conf.5.md)
or "man trackalert.conf".
