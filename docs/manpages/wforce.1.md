% WFORCE(1)
% Dovecot Oy
% 2016

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
configuring "siblings" in wforce.conf, and "spreading" all received reports
between all configured siblings. 

# OPTIONS
-c 
:    Act as a client, connecting to a wforce instance at the IP/Port 
     specified in the 'controlSocket' function in wforce.conf

-C,--config *FILE*
:    Load configuration from *FILE*.

-s
:    Run in foreground, but do not spawn a console. Use this switch to run
     wforce inside a supervisor (use with e.g. systemd and daemontools).

-d,--daemon
:    Operate as a daemon.

-e,--execute *CMD*
:    Connect to wforce and execute *CMD*.

-h,--help
:    Display a helpful message and exit.

-l,--local *ADDRESS*
:    Bind to *ADDRESS*.

# CONSOLE COMMANDS

The following commands can be run from the console when *wforce* is
started with the -c option.

* makeKey() - Returns a string to be used in the setKey() function in
  wforce.conf to authenticate sibling communications. All siblings
  must be configured with the same key.

* stats() - Returns statistics about the wforce process. For example:
  > stats()
  40 reports, 8 allow-queries, 40 entries in database

* siblings() - Returns information about configured siblings. For
  example:
  > siblings()
  Address                             Sucesses  Failures     Note
  192.168.1.79:4001                   18        7            
  192.168.1.30:4001                   25        0            
  192.168.1.54:4001                   0         0            Self

* showACL() - Returns the configured ACLs for the wforce server.

# BUGS
The 'spread' function of clustering means that as more servers are added to a 
cluster, incremental performance gains will be less each time, eventually
possibly leading to peformance degradation.


# SEE ALSO
wforce.conf(5)

