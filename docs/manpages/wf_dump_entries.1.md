= WF_DUMP_ENTRIES(1)
Open-Xchange 2020
doctype: manpage

## NAME
wf_dump_entries - Tool to dump the entries from the wforce stats
DBs to stdout or a file

## SYNOPSIS
wf_dump_entries [-u,--uri wforce uri] [-l,--local-addr address] [-p,--local-port port] [-w,--wforce-pwd password] [-f,--filename file] [-h,--help] 

## DESCRIPTION
**wf_dump_entries** connects to the specified wforce server, and asks
it to send a dump of all the stats DBs to itself over TCP/IP. It then
sends the results to stdout or a file. 

**wf_dump_entries** firstly acts as a TCP client, connecting to wforce
and invoking the 'dumpEntries' command. Then it acts as a server,
receiving the dumped entries, and printing them to stdout or to a file.

## SCOPE
**wf_dump_entries** requires a running wforce daemon to connect
to.

## OPTIONS
-u *URI*
:    Provides the URI of the wforce daemon, ideally without a path or
     query. For example: http://127.0.0.1:8084/. The query
     "?command=dumpEntries" will be appended to the command if the URI
     does not end with "dumpEntries".

-w,--wforce-pwd *PASSWORD*
:    The password to use for basic authentication to wforce.

-l,--local-addr *IP ADDRESS*
:    The IP address to use to send the entries to. This must be a
     configured IP address on the localhost. The wf_dump_entries program
     will listen on this IP address.

-p,--local-port *PORT*
:    The port number to use when sending the entries. The
     wf_dump_entries program will listen on this port.

-f,--filename *FILENAME*
:    Send the dumped entries to the specified file instead of stdout.

-h,--help
:    Display a helpful message and exit.

## SEE ALSO
wforce(1)

