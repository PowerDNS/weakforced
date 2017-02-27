The files in this directory are used to generate data to feed into wforce,
primarily to create data to populate named report sinks (typically
logstash/elasticsearch) with report data.

The following files are used:
 * gen_success_reports.lua - This is a file to be used by wrk2 in order to construct "random" data about successful login attempts to send to wforce as reports
 * gen_fail_reports.lua - This is a file to be used by wrk2 in order to construct "random" data about failed login attempts to send to wforce as reports
 * wforce_elastic.conf - This is a very simple (no policy) wforce config file that just sends reports to a logstash instance on localhost:14501. This is not the standard logstash port because I run logstash and elasticsearch in a Docker container.
 * send_reports.py - This is a python script that start wforce, and send reports to it using wrk2, which must be installed and in the path.

Wrk2 can be found at https://github.com/giltene/wrk2. It is also a very good tool for loadtesting wforce, and the gen_xxxx_reports.lua scripts are a good place to start if you wish to do so.
