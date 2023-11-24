# Release Notes for OX Abuse Shield 2.6.1

## Bug Fixes/Changes

* Fix issue where wforce was complaining about not being able to create tmp file on startup
* Fix timing issue whereby the webserver was not started before syncDB leading to syncDone failures
* Use debian bullseye-slim in wforce docker image to save over 100MB in image size
* Fix issue in wforce docker image where the default config file was overridden with a volume mount but not used

## Fix Wforce complaint about not being able to create temporary files on startup

Wforce 2.6.x uses an HTTP library which creates temporary directories for file upload on startup, 
by default in the current working directory, which for wforce is the config directory. For packaged
installation of wforce, this is /etc/wforce, which is typically not writable by wforce itself, leading
to errors. This fix changes the directory for those temporary files to /tmp/wforce.

## Fix timing issue with webserver and syncDB

In rare cases when starting up, the syncDB command may start, replicate from another wdforce instance, 
and complete, before the webserver had finished initializing. This would cause the syncDone command from
the other wforce instance to fail. This fix forces wforce to wait until the webserver is ready before
starting the syncDB checks.

## Fix issue in wforce docker image where the default config file was overridden by a volume mount by not used

The wforce docker image documentation states that a volume mount can be used to specify a custom config file
in /etc/wforce/wforce.conf, however this was not actually the case. The file was only used if the environment
variable WFORCE_CONFIG_FILE was also set, which is incorrect, because that variable is only supposed to be used
to specify a *new* location for the config file. This fix ensures that whenever a volume mount correctly
mounts a custom /etc/wforce/wforce.conf file, it is both used, and a log message is output stating that it
is being used.
