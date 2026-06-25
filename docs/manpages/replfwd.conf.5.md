= REPLFWD.CONF(1)
Open-Xchange 2019
doctype: manpage

// <!-- {% raw %} -->

## NAME
replfwd.conf - configuration file for replfwd daemon

## DESCRIPTION
This file is read by **replfwd** and is a Lua script containing Lua
commands to implement configuration of the replfwd daemon.

An alternative version of this file can be specified with

    replfwd -C private_replfwd.conf ...

## CONFIGURATION FUNCTIONS

The following functions are for configuration of replfwd:

* setSiblings(\<list of IP[:port[:protocol]]\>) - Set the list of siblings to which
  stats db and blacklist/whitelist data should be replicated. If port
  is not specified it defaults to 4001.  If protocol is not specified
  it defaults to udp. For example:
  
        setSiblings({"127.0.1.2", "[::1]:4004", "127.0.2.23:4004:tcp"})

* setSiblingsWithKey(\<list of {IP/Host[:port[:protocol]], Encryption Key\>) - Identical
  to setSiblings() except that it allows an encryption key to be specified for
  each sibling. For example:

        setSiblingsWithKey({{"127.0.1.2", "Ay9KXgU3g4ygK+qWT0Ut4gH8PPz02gbtPeXWPdjD0HE="},{"127.0.1.3:4004:tcp", "KaiQkCHloe2ysXv2HbxBAFqHI4N8+ahmwYwsbYlDdF0="}})

* addSibling(\<IP[:port[:protocol]]\>) - Add a sibling to the list to which all
  stats db and blacklist/whitelist data should be replicated.  If port
  is not specified it defaults to 4001. If protocol is not specified
  it defaults to udp. For example:
  
        addSibling("192.168.1.23")
        addSibling("[::1]:4001:udp")
        addSibling("192.168.1.23:4003:tcp")

* addSiblingWithKey(\<IP[:port[:protocol]]\>, \<Encryption Key\>) - Identical
  to addSibling(), except that an encryption key is specified to enable per-sibling
  encryption.For example:

        addSiblingWithKey("192.168.1.23", "Ay9KXgU3g4ygK+qWT0Ut4gH8PPz02gbtPeXWPdjD0HE=")

* removeSibling(\<IP/Host[:port[:protocol]]\>) - Remove a sibling to the list to which all
  stats db and blacklist/whitelist data should be replicated.  Use [] to enclose
  IPv6 addresses. If port
  is not specified it defaults to 4001. If protocol is not specified
  it defaults to udp. This function is safe to call while wforce is running. For example:

      removeSibling("192.168.1.23")
      removeSibling("sibling1.example.com:4001:udp")
      removeSibling("[::1]:4003:tcp")
  
* siblingListener(\<IP[:port]\>) - Listen for reports from siblings or
  replication forwarders on the specified IP address and port.  If
  port is not specified it defaults to 4545. Use [] to specify an IPv6 address.
  Replfwd will always listen on both UDP and TCP ports. For example:
  
        siblingListener("0.0.0.0:4001")

* webserver(\<IP:port\>, \<password\>) - (*deprecated - see addListener() instead*) Listen for HTTP commands on the
  specified IP address and port. This is currently only used for
  prometheus stats. The password is used to authenticate
  client connections using basic authentication. For example:
  
        webserver("0.0.0.0:8084", "super")

* addListener(\<IP:port\>, \<useSSL\>, \<cert file\>, \<key file\>, \<options\>) - (*replacement for webserver()*) Listen
  for HTTP commands on the specified IP address and port. If useSSL is true, then HTTPS must be used, and cert_file and
  key file are used, otherwise they are empty. Options contains a list of key value pairs to configure the TLS connection;
  these follow the command line option names in https://www.openssl.org/docs/manmaster/man3/SSL_CONF_cmd.html.
  For example, "min_protocol" to set the minimum TLS protocol version. You can add as many listeners as you choose. For example:

        addListener("0.0.0.0:8084", false, "", "", {})
        addListener("1.2.3.4:1234", true, "/etc/wforce/cert.pem", "/etc/wforce/key.pem", {minimum_protocol="TLSv1.2"})
        addListener("[::1]:9000", true, "/etc/wforce/cert.pem", "/etc/wforce/key.pem", {minimum_protocol="TLSv1.3"})

* setWebserverPassword(\<Password\>) - (*replacement for webserver password parameter*) Sets the basic authentication
  password for access to the webserver. This has been decoupled from the addListener() command because multiple listeners
  can now be created, which was not previously possible. For example:

        setWebserverPassword("foobar")

* setKey(\<key\>) - Use the specified key for encrypting/decrypting 
  connections from siblings and other replication forwarders (if a per-sibling
  key is not defined). The key is typically copied from the wforce.conf file of one of the siblings. 
  Returns false if the key could not be set (e.g. invalid base64). 
  For example:
  
        if not setKey("Ay9KXgU3g4ygK+qWT0Ut4gH8PPz02gbtPeXWPdjD0HE=")
        then
          ...

* setSiblingConnectTimeout(\<timeout ms\>) - Sets a timeout in milliseconds
  for new connections to siblings. Defaults to 5000 ms (5 seconds). For example:

        setSiblingConnectTimeout(1000)

* setMaxSiblingQueueSize(\<size\>) - Sets the maximum size of the
  send and receive queues for replication events waiting to be processed.
  Defaults to 5000. This is to handle short-term spikes in load/latency.

        setMaxSiblingQueueSize(10000)

* setNumSiblingThreads(\<num threads\>) - Set the number of threads in
  the pool used to process reports received from siblings and
  replication forwarders. Defaults to 2 if not specified.
  For example:
  
        setNumSiblingThreads(2)

* removeReplicationForwarderDest(\<IP:port\>) - Remove a replication 
  forwarder destination to the specified IP address and port. If port is 
  missing, defaults to 4545.
  For example:

        removeReplicationForwarderDest("127.0.0.4:4545")
        removeReplicationForwarderDest("[::1]:9045")

* addReplicationForwarderDest(\<IP:port[:<protocol>]\>, \<replicate statsdb\>,
  \<replicate blwl\>) - Add a replication forwarder destination on the
  specified IP address and port (and optional protocol, which defaults to "udp").
  If port is missing, defaults to 4545.
  If "replicate statsdb" is true, then replicate statsdb events. If
  "replicate blwl" is true, then replicate blacklist/whitelist events.
  For example:

        addReplicationForwarderDest("127.0.0.4:4545", true, true)

* addReplicationForwarderDestWithKey(\<IP:port[:<protocol>]\>, \<key\>, \<replicate statsdb\>,
  \<replicate blwl\>) - Add a replication forwarder destination on the
  specified IP address and port (and optional protocol, which defaults to "udp"),
  using the specified encryption key. If port is missing, defaults to 4545.
  If "replicate statsdb" is true, then replicate statsdb events. If
  "replicate blwl" is true, then replicate blacklist/whitelist events.
  For example:

        addReplicationForwarderDestWithKey("127.0.0.4:4545", "Ay9KXgU3g4ygK+qWT0Ut4gH8PPz02gbtPeXWPdjD0HE=", true, true)

* setReplicationForwarderDestsWithKey(\<list of {IP/Host[:port[:protocol]], \<Encryption Key\>, \<replicate statsdb\>,
  \<replicate blwl\>) - Allows the replication forwarders to be set in a single operation. Includes
  the encryption key for each forwarder; to use the global encryption key, specify an empty string. For the replicate 
  statsdb and wlbl parameters, specify a string of either "true" or "false". For example:

      setReplicationForwarderDestsWithKey({{"127.0.1.2", "Ay9KXgU3g4ygK+qWT0Ut4gH8PPz02gbtPeXWPdjD0HE=", "true", "false"}, {"[::1]:4004:tcp", "KaiQkCHloe2ysXv2HbxBAFqHI4N8+ahmwYwsbYlDdF0=", "true", "true"}})

* setReplicationForwarderSrcs(\<list of IP Addresses\>) - Set the list of replication
  forwarders from which replication events will be accepted. For example:

      setReplicationForwarderSrcs({"127.0.1.2", "[::1]", "127.0.2.23"})

* addReplicationForwarderSrc(\<IP Address\>) - Add a replication
  forwarder source on the specified IP address. Only events from
  configured replication forwarder sources will be accepted. For
  example:

        addReplicationForwarderSrc("127.0.0.4")

* removeReplicationForwarderSrc(\<IP Address\>) - Remove a replication
  forwarder source from the specified IP address. For example:

        removeReplicationForwarderSrc("127.0.0.4")

* setMetricsNoPassword() - Disable password protection for the /metrics endpoint.

## FILES
*/etc/wforce/replfwd.conf*

## SEE ALSO
replfwd(1)

// <!-- {% endraw %} -->
