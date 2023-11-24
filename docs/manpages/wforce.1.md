% WFORCE(1)
% Open-Xchange
% 2018

# NAME
**wforce** - daemon to detect brute-force login attempts and enforce other policies for logins

# SYNOPSIS
wforce [*OPTION*]... 

# DESCRIPTION
**wforce** implements a simple HTTP server that accepts JSON formatted commands 
that report successful/unsuccessful logins and query whether a login should be 
allowed to proceed. 

**wforce** can act as both a client and server. As a server it typically runs
under systemd control, although it can also be run as a traditional daemon
or in 'interactive' mode. As a client, it connects to a wforce server and
provides the same interactive commands.

**wforce** is scriptable in Lua, see the wforce.conf file for a simple example,
and wforce.conf.example for a more sophisticated example. In fact all
configuration is done using the Lua language, as wforce.conf is simply a
Lua script.

# SCOPE
**wforce** depends on the systems performing login authentication to integrate
with it using the HTTP/JSON API. Example clients of the API include Dovecot
and OX AppSuite.

**wforce** provides a simple clustering mechanism through the process of 
configuring "siblings" in wforce.conf. All modifications to the
blacklists or the string stats DB (either from Lua or the REST API)
will be replicated to all configured siblings. Replication uses the
UDP protocol, so if siblings are not on a local subnet, you should
ensure that any firewalls forward UDP on the configured ports.

# OPTIONS
-c 
:    Act as a client, connecting to a wforce instance at the IP/Port 
     specified in the 'controlSocket' function in wforce.conf. A
     custom configuration file can be specified.

-C,--config *FILE*
:    Load configuration from *FILE*.

-R,--regexes *FILE*
:    Read device parsing regexes from *FILE* (usually regexes.yaml).

-s
:    Run in foreground, but do not spawn a console. Use this switch to run
     wforce inside a supervisor (use with e.g. systemd and daemontools).

-d,--daemon
:    Operate as a daemon.

-e,--execute *CMD*
:    Connect to wforce and execute *CMD*.

-f,--facility *FACILITY NAME*
:    Log using the specified facility name, e.g. local0

-l,--loglevel <0-7>
:    Logs sent to stdout will be filtered according to the specified log level,
     matching the equivalent syslog level (0 - Emerg to 7 - Debug).

-h,--help
:    Display a helpful message and exit.


# CONSOLE COMMANDS

The following commands can be run from the console when *wforce* is
started with the -c option.

* makeKey() - Returns a string to be used in the setKey() function in
  wforce.conf to authenticate sibling communications. All siblings
  must be configured with the same key.

		> makeKey()
		setKey("CRK+jKBpzXNLmM2A4C7OpFCBxiwpYlreCWgGEAIKAQI=")


* stats() - Returns statistics about the wforce process. For example:

		> stats()
		40 reports, 8 allow-queries (% denies)

* siblings() - Returns information about configured siblings. For
  example:

        > siblings()
        Address                             Send Successes  Send Failures  Rcv Successes   Rcv Failures     Note
        127.0.0.1:4001                      0               0              17              0                
        127.0.0.1:4002                      0               0              0               0                Self

* showNamedReportSinks() - Returns information about configured report
  sinks. For example: 

		> showNamedReportSinks()
		Name            Address                             Sucesses  Failures
		trackalert      192.168.1.79:4501                   18        0
		trackalert      192.168.1.30:4501                   19        0
		elasticsearch   10.22.2.15:4501                     18        0
		elasticsearch   10.22.2.16:4501                     19        0

* showReportSinks() - Deprecated - use showNamedReportSinks() instead. Returns
  information about configured report sinks. For example:

		> showReportSinks()
		Address                             Sucesses  Failures
		192.168.1.79:4501                   18        7
		192.168.1.30:4501                   25        0

* showStringStatsDB() - Returns information about configured stats
  databases. This includes the DB Name/number of shards, whether it is
  configured for replication, the size and number of windows, the
  maximum size, the current size, and finally all the configured
  fields and their types. For example:

		> showStringStatsDB()
		DB Name/Shards Repl? Win Size/No Max Size  Cur Size  Field Name       Type
		MyDB1/1        yes   1/15        524288    0         countLogins      int
		                                                     diffPasswords    hll
		MyDB2/10       no    600/6       5000      2093      diffIPs          hll

* showACL() - Returns the configured ACLs for the wforce server.

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

* showWebHooks() - Returns information about configured webhooks. For
  example: 

		> showWebHooks()
		ID        Successes Failures  URL                            Events
		1         5         2         http://localhost:8080/webhook/ report allow

* showCustomWebHooks() - Returns information about configured custom
  webhooks. For example:

  	    	> showCustomWebHooks()
		ID        Name                 Successes Failures  URL
		1         mycustomhook         10         0         http://localhost:8080/webhook/regression

* showCustomEndpoints() - Returns information about configured custom
  endpoints. For example:

  	     	 > showCustomEndpoints()
		 Custom Endpoint                Send to Report Sink? 
		 custom1                        true
		 custom2                        false

* showPerfStats() - (Deprecated in favour of prometheus metrics - will
  be removed in a future version). Returns information about performance
  statistics. Stats beginning with WTW refer to the time that worker
  threads waited in a queue before running. Stats beginning with WTR
  refer to the time that worker threads took to run. Each stat is in a
  bucket, where each bucket represents a time range in ms,
  e.g. 0-1. A server that is not overloaded will have most stats in
  the 0-1 buckets. Stats are for the previous 5 minutes. For example:

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

* showCommandStats() - (Deprecated in favour of prometheus metrics -
  will be removed in a future version). Returns information about the
  number of REST 
  API commands that have been called, including custom
  endpoints. Stats are for the previous 5 mins, and due to the
  counting method, may be approximate when the numbers get very
  large. For example:

        > showCommandStats()
        addBLEntry=0
        allow=23942
        delBLEntry=0
        getBL=0
        getDBStats=0
        ping=300
        report=19232
        reset=24
        stats=92
        customEndpoint=2821

* showCustomStats() - (Deprecated in favour of prometheus metrics -
  will be removed in a future version). Returns information about
  custom stats that are
  incremented from Lua. Stats are for the previous 5 mins, and due to the
  counting method, may be approximate when the numbers get very
  large. For example:

        > showCustomStats()
        custom1=0
        custom2=8405

* reloadGeoIPDBs() - Reload all GeoIP DBs that have been
initialized. For example:

		> reloadGeoIPDBs
		reloadGeoIPDBs() successful

* showVersion() - Returns the current version of the wforce
  server. For example:

		> showVersion()
		wforce 1.2.0


# BUGS
The replication function of clustering means that as more servers are added to a 
cluster, incremental performance gains may be less each time, eventually
possibly leading to performance degradation. This is because each
server keeps a full copy of the stats DBs and the blacklists, and
changes to those are replicated to all siblings. This can be mitigated by
partitioning siblings into smaller clusters that do not share
information, at the expense of missing potential abuse activity. 

# SEE ALSO
wforce.conf(5) wforce_webhook(5) wforce_api(7)

