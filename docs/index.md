# Weakforced Documentation

Welcome to the documentation site for [weakforced](https://github.com/PowerDNS/weakforced). Weakforced is an open-souce anti-abuse engine for authentication systems, acting as a policy decision point for email systems, web portals and any other type of system that is open to authentication abuse on the Internet. Weakforced is in production at sites hosting email for over 20 million customers, so it scales extremely well.

Here you will find all the manpages for weakforced, which are the primary documentation source. You'll also find an overview of the main features.

These pages are for detailed technical documentation; for details about building, getting started, and testing see the main repo [README](https://github.com/PowerDNS/weakforced/blob/master/README.md).

Go [here](https://github.com/PowerDNS/weakforced/releases) for release information.

# Wforce Daemon

The wforce daemon is the main point of integration with authentication systems, and several products are pre-integrated with it, including [Dovecot](https://wiki.dovecot.org/Authentication/Policy) and [OX AppSuite](https://documentation.open-xchange.com/7.10.3/middleware/security_and_encryption/weakforced_connector.html).

* [Wforce Daemon](manpages/wforce.1.md)
* [Wforce Config File](manpages/wforce.conf.5.md)
* [Wforce Webhook Configuration](manpages/wforce_webhook.5.md)
* [REST API](https://documentation.open-xchange.com/components/weakforce-core/2.0.0/)

## Main Features

* Lua Policy Engine - Create policy using Lua; infinitely flexible and very quick.
* Statistics DB - A built-in, in-memory statistics DB that stores information about authentication activity using one of the three built-in data types (integer, hyperloglog and countmin), using sliding time windows. You can configure multiple statistics DBs, and each DB can be sharded within each instance for better performance.
* Persistent Black/Whitelists - Stored in Redis and modified and retrieved using either Lua or the REST API.
* Replication - The statistics databases and black/whitelists can be replicated between wforce instances.
* DNS Lookup Support - Built-in support for DNS lookups from Lua, using the GetDNS library for extremely fast and low-latency lookups. Includes support for looking up RBL-formatted zones.
* Highly Multithreaded - Scales extremely well on multicore servers.
* Webhook Support - Provides built-in support for webhooks on core events, as well as custom webhooks from Lua to send any kind of data anywhere.
* REST API - All integration uses the built-in REST API. Custom REST API endpoints can be created in Lua, for endless extensibility.
* Prometheus Support - Native Support for prometheus metrics on the /metrics endpoint.
* GeoIP Support - Retrieve location information of IP addresses using the GeoIP2 API.

# Trackalert Daemon

The trackalert daemon provides a mechanism to run arbitrary Lua code based on either a wforce "report" event and/or on a scheduled basis. This is often used to trigger lookups into databases such as Elasticsearch to find anomalous login events.

Wforce is pre-integrated with trackalert - you can just create a webhook on "report" events, providing the address and authentication details for the trackalert server.

* [Trackalert Daemon](manpages/trackalert.1.md)
* [Trackalert Config File](manpages/trackalert.conf.5.md)
