% WFORCE-REPORT-API(5)
% Open-Xchange
% 2018

# NAME
**wforce-report-api** - REST API to query reports stored in elasticsearch

# DESCRIPTION
The wforce-report-api package consists of a web application running under gunicorn, providing REST API endpoints to query the reports stored in elasticsearch. It also provides endpoints to modify reports, including "confirming" user logins and "forgetting" devices.

This manpage describes the configuration of the wforce-report-api package. Note that for production usage, we recommend deploying a web proxy such as nginx in front of the gunicorn server.

# REPORT-API CONFIGURATION

The report-api web application is configured using the file wforce-report-api-instance.conf.

The following is the default configuration:

```
ELASTICSEARCH_URL = ["http://127.0.0.1:9200"]
ELASTICSEARCH_TIMEOUT = 1
ELASTICSEARCH_INDEX = "logstash-wforce*"
AUTH_PASSWORD = "secret"
LOG_LEVEL = logging.INFO
LOG_FORMAT = '%(asctime)s - %(name)s - %(levelname)s - %(message)s'
DEVICE_UNIQUE_ATTRS = ['os.family', 'browser.family', 'imapc.family', 'device.family', 'app.name']
TRACKALERT_REDIS = True
TRACKALERT_SERVER = 'localhost'
TRACKALERT_PORT = 8085
TRACKALERT_PASSWORD = 'super'
```

These are described as follows:

* ELASTICSEARCH_URL - This is the URL of the elasticsearch server or cluster.

* ELASTICSEARCH_TIMEOUT - The timeout in seconds before giving up on an elasticsearch search.

* ELASTICSEARCH_INDEX - The elasticsearch index pattern to use; this is unlikely to need changing.

* AUTH_PASSWORD - The password required for basic authentication to the report api by clients.

* LOG_LEVEL - The log level to use for report api web application.

* LOG_FORMAT - The format of the logging used by the report api web application.

* DEVICE_UNIQUE_ATTRS - This is required for removing items from the Redis device cache in trackalert. This is unlikely to need changing unless you have modified trackalert to cache devices in Redis using different fields. This configuration key is required if TRACKALERT_REDIS is True.

* TRACKALERT_REDIS - Whether or not trackalert is caching device information in Redis. This determines whether the report api webserver removes devices from the Redis cache, which it does by contacting a trackalert instance.

* TRACKALERT_SERVER - The hostname of a trackalert server. Note that this can be any trackalert server connected to Redis. This configuration key is required if TRACKALERT_REDIS=True.

* TRACKALERT_PORT - The port number of the trackalert server.

* TRACKALERT_PASSWORD - The password to use when connecting to trackalert. This configuration key is required if TRACKALERT_REDIS=True.

# GUNICORN CONFIGURATION

The file wforce-report-api-web.conf is used to configure gunicorn. The following are the relevant settings that can be configured in the file:

* bind - The hostname and port that gunicorn binds to. Defaults to "['127.0.0.1:5000']".

* workers - The number of workers to spawn. Defaults to "multiprocessing.cpu_count() * 2".

* threads - The number of threads per worker. Defaults to 8.

* timeout - Timeout (in seconds) between gunicorn and the report api web application. Defaults to 30.

* loglevel - The loglevel to use for gunicorn. Defaults to 'info'.

# FILES
*/etc/wforce-report-api/wforce-report-api-instance.conf*

*/etc/wforce-report-api/wforce-report-api-web.conf*

# SEE ALSO
trackalert(1) trackalert.conf(5) wforce(1) wforce.conf(5) 
