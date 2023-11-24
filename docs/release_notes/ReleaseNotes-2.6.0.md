# Release Notes for OX Abuse Shield 2.6.0

## New Features

* REST API supports TLS/HTTPS natively
* Multiple REST API listeners can be configured
* Outbound HTTPS connection TLS behaviour is configurable
* Build on Debian Bullseye
* Remove support for building on Debian Stretch

## Bug Fixes/Changes

* Fix issue where building of geoip2 functionality was dependent on legacy geoip library being installed

## REST API Supports TLS/HTTPS natively

The `webserver()` configuration command is now deprecated, and is replaced with `addListener()`,
which enables both TLS and non-TLS listeners to be created, as well as enabling multiple listeners
to be created oncurrently. The new command `setWebserverPassword()` is used to set the password
for the REST API (previously this was set as part of the `webserver()` command).

An example listener without TLS:
* `addListener("0.0.0.0:8084", false, "", "", {})`

An example listener with TLS:
* `addListener("1.2.3.4:1234", true, "/etc/wforce/cert.pem", "/etc/wforce/key.pem", {minimum_protocol="TLSv1.2"})`
`

For more details, see the man page for wforce.conf.

## Outbound HTTPS connection TLS behaviour is configurable

Various options for the configuration of outbound HTTPS connections are now supported, specifically:

* Mutual TLS Authentication - `setCurlClientCertAndKey()` is used to specify the location of a client certificate 
  and key for mTLS.
* Using a different CA for checking server certificates - `setCurlCABundleFile()` is used to specify the location
  of a file containing certs to use for this purposes.
* Disable checking peer certificates - `disableCurlPeerVerification()` disables checking of peer certificates 
  (not recommended except for debugging).
* Disable peer certificate hostname checking - `disableCurlHostVerification()` disables checking of the hostname
  in peer certificates (not recommended except for debugging).

## Build on Debian Bullseye

Support for building on debian bullseye.