= REPLFWD(1)
Open-Xchange 2019
doctype: manpage

## NAME
replfwd - daemon to forward replication events between wforce clusters

## SYNOPSIS
replfwd [*OPTION*]... 

## DESCRIPTION
**replfwd** is a daemon that will forward replication events between
wforce clusters and other replication forwarders, and vice-versa.

**replfwd** configuration uses Lua, see the replfwd.conf file for a
simple example.

## SCOPE
**replfwd** is not standalone - it is dependent on a wforce cluster to
forward received events from other replication forwarders, and on
other replication forwarders to send events to.

**replfwd** would typically be configured with all the other members
  of the local wforce cluster, as well as the replication forwarders
  for all remote clusters.

## OPTIONS
-C,--config *FILE*
:    Load configuration from *FILE*.

-s
:    Run in foreground, and notify systemd when started. Use this switch to run
     replfwd inside a supervisor (use with e.g. systemd and daemontools).

-D,--docker
:    Enable timestamps in log messages for running in docker containers.

-l,--loglevel
:    Log level as an integer. 0 is Emerg, 7 is Debug. Defaults to 6 (Info). Only applies
     to stdout logging.

-d,--daemon
:    Operate as a daemon.

-f,--facility *FACILITY NAME*
:    Log using the specified facility name, e.g. local0

-k,--key
:    Generate a key to be used as the encryption key for messages,
     print to stdout and exit. Note that this is not typically
     expected to be used.

-h,--help
:    Display a helpful message and exit.

## BUGS

## SEE ALSO
replfwd.conf(5)
