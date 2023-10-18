# Wforce Docker Image Documentation

This page documents the usage of the wforce docker image.

## Obtaining the image

The image is hosted on docker hub in the powerdns/wforce repository.

The image must be retrieved with a specific version tag, There is no
version tagged "latest"; check the [wforce github releases page](https://github.com/PowerDNS/weakforced/releases) for the latest
version tags. For example:

````
% docker pull powerdns/wforce:v2.2.2
````

## Using the image

The image supports three different use cases:

1) (The default) - Running the wforce daemon
2) Running the trackalert daemon
3) Running the report_api webservice

## Running the wforce daemon

### Wforce Quickstart

To quickly get the wforce docker image running as a container running wforce, you need to do the following:

* Set the WFORCE_HTTP_PASSWORD environment variable (it won't start if that is not set).
* Start the container, specifying port 8084 to expose the REST API (that port can be changed, see below).

For example:

````
% export WFORCE_HTTP_PASSWORD=secret
% docker run --env WFORCE_HTTP_PASSWORD -p 8084:8084 powerdns/wforce:v2.4.0
Note you are using the default config file - to take full advantage of the capabilities of weakforced, you should specify a custom config file with the WFORCE_CONFIG_FILE environment variable.
Starting /usr/bin/wforce -D
[2020-05-05 10:58:10,645] Read configuration from '/etc/wforce/wforce.conf'
[2020-05-05 10:58:10,653] Opened MMDB database /usr/share/GeoIP/GeoLite2-City.mmdb (type: GeoLite2-City version: 2.0)
[2020-05-05 10:58:10,659] ACL allowing queries from: 100.64.0.0/10, 169.254.0.0/16, 192.168.0.0/16, 10.0.0.0/8, 127.0.0.0/8, fc00::/7, ::1/128, 172.16.0.0/12, fe80::/10
[2020-05-05 10:58:10,661] Starting stats reporting thread
[2020-05-05 10:58:10,663] WforceWebserver launched on 0.0.0.0:8084
[2020-05-05 10:58:10,665] Accepting control connections on 0.0.0.0:4004
````

Note the warning about using the default config file - the docker image ships with a configuration that blocks brute-force attacks using a very simple policy. To use the powerful features of wforce you'll need to provide your own configuration file(s).

There are some other features that can be configured using environment variables:

* WFORCE_LOGSTASH_URL - If you set this to the URL of a logstash service that is configured with an HTTP input, you can send all report and allow data to the logstash instance. You can make use of the [sample logstash configuration](https://github.com/PowerDNS/weakforced/blob/master/docker/logstash/config/logstash.conf) as well as the [sample kibana reports and dashboards](https://github.com/PowerDNS/weakforced/blob/master/elk/kibana/kibana_saved_objects.json) to be able to store and view the data in elasticsearch.
* WFORCE_HTTP_PORT - You can change the port that wforce listens on for REST API requests.

### Advanced Configuration

To make use of the powerful features of wforce, you'll need to provide your own configuration file. You can do this in several ways, for example:

* Mount your own configuration file to replace /etc/wforce/wforce.conf - You can use a variety of mechanisms to do this, such as docker configs, or in a Kubernetes environment a volume populated with a ConfigMap.
* Use the WFORCE_CONFIG_FILE environment variable to specify a different configuration file. Be aware that wforce uses the location of the configuration file to look for the mandatory regexes.yaml file (which lives in /etc/wforce/regexes.yaml), so use a different directory at your own risk.

### Verbose Logging

To turn on verbose logging for the wforce daemon, set the WFORCE_VERBOSE environment variable to 1, e.g.:

````
% export WFORCE_VERBOSE=1
% docker run --env WFORCE_HTTP_PASSWORD --env WFORCE_VERBOSE -p 8084:8084 powerdns/wforce:v2.4.0
````

Warning: this generates a lot of logs - you have been warned!

## Running the trackalert daemon

## Quickstart

To run trackalert instead of wforce, set the environment variable
TRACKALERT=1.

You'll also need to set the following environment variables:

* Set the TRACKALERT_HTTP_PASSWORD environment variable (it won't start if that is not set).
* Start the container, specifying port 8085 to expose the REST API (that port can be changed, see below).

For example:

```
% export TRACKALERT=1
% export TRACKALERT_HTTP_PASSWORD=foo
% docker run --env TRACKALERT --env TRACKALERT_HTTP_PASSWORD -p 8085:8085 powerdns/wforce:v2.4.0
Note you are using the default config file - to take full advantage of the capabilities of trackalert, you should specify a custom config file with the TRACKALERT_CONFIG_FILE environment variable or just replace /etc/wforce/trackalert.conf with your own config file.
Starting /usr/bin/trackalert -D
[2020-06-29 16:43:03,961] Read configuration from '/etc/wforce/trackalert.conf'
[2020-06-29 16:43:03,964] ACL allowing queries from: 127.0.0.0/8, 100.64.0.0/10, 169.254.0.0/16, 172.16.0.0/12, 10.0.0.0/8, 192.168.0.0/16, ::1/128, fc00::/7, fe80::/10
[2020-06-29 16:43:03,964] Starting stats reporting thread
[2020-06-29 16:43:03,964] Accepting control connections on 127.0.0.1:4005
[2020-06-29 16:43:03,964] WforceWebserver launched on 0.0.0.0:8085
```

Note the warning about using the default config file - the default
trackalert configuration file is only an example. To use the powerful features of trackalert you'll need to provide your own configuration file.

There are some other features that can be configured using environment variables:

* TRACKALERT_HTTP_PORT - You can change the port that trackalert
listens on for REST API requests.

### Verbose Logging

To turn on verbose logging for the trackalert daemon, set the WFORCE_VERBOSE environment variable to 1, e.g.:

````
% export WFORCE_VERBOSE=1
% docker run --env TRACKALERT --env TRACKALERT_HTTP_PASSWORD --env WFORCE_VERBOSE -p 8084:8084 powerdns/wforce:v2.4.0
````

Warning: this generates a lot of logs - you have been warned!

## Running the report_api webservice

This is no longer supported in the wforce image (>2.8.0)
