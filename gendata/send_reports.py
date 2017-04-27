#!/usr/bin/env python
#
# Shell-script style.

import os
import requests
import shutil
import subprocess
import sys
import tempfile
import time

WEBPORT = '8084'
APIKEY = 'super'

cmd1 = ("../wforce -C ./wforce_elastic.conf -R ../regexes.yaml").split()
wrksuccesscmd = ("wrk -c 2 -d 60 -t 2 -s ./gen_success_reports.lua -R 30 http://127.0.0.1:8084").split()
wrkfailcmd = ("wrk -c 2 -d 60 -t 2 -s ./gen_fail_reports.lua -R 20 http://127.0.0.1:8084").split()

# Now run wforce and the tests.
print "Launching wforce..."
print ' '.join(cmd1)
proc1 = subprocess.Popen(cmd1, close_fds=True)
wforcepid = proc1.pid

print "Waiting for webserver port to become available..."
available = False
for try_number in range(0, 10):
    try:
        res = requests.get('http://127.0.0.1:%s/' % WEBPORT)
        available = True
        break
    except:
        time.sleep(0.5)

if not available:
    print "Webserver port not reachable after 10 tries, giving up."
    proc1.terminate()
    proc1.wait()
    sys.exit(2)

print "Sending Reports..."
wrkproc = subprocess.Popen(wrksuccesscmd, close_fds=True)
wrkproc.wait()
time.sleep(10)
wrkproc = subprocess.Popen(wrkfailcmd, close_fds=True)
wrkproc.wait()
time.sleep(10)
print "Done sending reports..."
proc1.terminate()
proc1.wait()
print "Exiting"
sys.exit(0)
