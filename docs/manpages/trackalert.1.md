% TRACKALERT(1)
% Dovecot Oy
% 2017

# NAME
**trackalert** - daemon to track and alert on long-term abuse trends
for logins

# SYNOPSIS
trackalert [*OPTION*]... 

# DESCRIPTION
**trackalert** implements a simple HTTP server that accepts JSON
formatted commands that report successful/unsuccessful logins.

**trackalert** can act as both a client and server. As a server it
typically runs under systemd control, although it can also be run as a
traditional daemon or in 'interactive' mode. As a client, it connects
to a trackalert server and provides the same interactive commands.

**trackalert** is scriptable in Lua, see the trackalert.conf file for
a simple example. In fact, all configuration is done using the Lua
language, as trackalert.conf is simply a Lua script.

# SCOPE
**trackalert** depends on a system to feed it login reports, which
will typically be wforce (configured with a webhook that triggers only
on "report") integrated with it using the HTTP/JSON API.

# OPTIONS
-c 
:    Act as a client, connecting to a trackalert instance at the IP/Port 
     specified in the 'controlSocket' function in trackalert.conf. A
     custom configuration file can be specified.

-C,--config *FILE*
:    Load configuration from *FILE*. Note that trackalert will chdir
	 to the directory where the configuration file is located.

-R,--regexes *FILE*
:    Read device parsing regexes from *FILE* (usually regexes.yaml).

-s
:    Run in foreground, but do not spawn a console. Use this switch to run
     trackalert inside a supervisor (use with e.g. systemd and daemontools).

-d,--daemon
:    Operate as a daemon.

-e,--execute *CMD*
:    Connect to trackalert and execute *CMD*.

-h,--help
:    Display a helpful message and exit.


# CONSOLE COMMANDS

The following commands can be run from the console when *trackalert* is
started with the -c option.

* makeKey() - Returns a string to be used in the setKey() function in
  trackalert.conf to authenticate sibling communications. All siblings
  must be configured with the same key.

		> makeKey()
		setKey("CRK+jKBpzXNLmM2A4C7OpFCBxiwpYlreCWgGEAIKAQI=")


* stats() - Returns statistics about the trackalert process. For example:

		> stats()
		40 reports

* showACL() - Returns the configured ACLs for the trackalert server.

		> showACL()
		127.0.0.0/8
		10.0.0.0/8
		100.64.0.0/10
		169.254.0.0/16
		192.168.0.0/16
		172.16.0.0/12
		::1/128
		fc00::/7
		fe80::/10

* showCustomWebHooks() - Returns information about configured custom
  webhooks. For example:

		> showCustomWebHooks()
		ID        Name                 Successes Failures  URL
		1         mycustomhook         10         0         http://localhost:8080/webhook/regression

* showPerfStats() - Returns information about performance
  statistics. Stats beginning with WTW refer to the time that worker
  threads waited in a queue before running. Stats beginning with WTR
  refer to the time that worker threads took to run. Each stat is in a
  bucket, where each bucket represents a time range in ms,
  e.g. 0-1. A server that is not overloaded will have most stats in
  the 0-1 buckets. For example:

		> showPerfStats()
		WTW_0_1=2939287
		WTW_1_10=9722
		WTW_10_100=4
		WTW_100_1000=0
		WTW_Slow=0
		WTR_0_1=2939229
		WTR_1_10=2837
		WTR_10_100=131
		WTR_100_1000=0
		WTR_Slow=0

* reloadGeoIPDBs() - Reload all GeoIP DBs that have been
initialized. For example:

		> reloadGeoIPDBs
		reloadGeoIPDBs() successful

* showVersion() - Returns the current version of the trackalert
  server. For example:

		> showVersion()
		trackalert 1.2.0


# SEE ALSO
trackalert.conf(5) trackalert_api(7)

