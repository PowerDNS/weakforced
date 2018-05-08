import requests
import socket
import subprocess
import sys
import time
import json
from test_helper import ApiTestCase

class TestSyncDBs(ApiTestCase):
    def test_SyncDBs(self):
        for i in range(42):
            r = self.reportFunc('subbaddie%s' % i, '128.0.0.%s' % i, "1234", False)
            r.json()

        res1 = self.writeCmdToConsole("showStringStatsDB()");
        res1_ss = res1.split("DB Name", 1)[1]
        
        time.sleep(11);
        
        cmd3 = ("../wforce/wforce -C ./wforce3.conf -R ../wforce/regexes.yaml").split()
        proc3 = subprocess.Popen(cmd3, close_fds=True)
        time.sleep(5)

        res2 = self.writeCmdToConsole3("showStringStatsDB()");
        res2_ss = res2.split("DB Name", 1)[1]

        self.assertEqual(res1_ss == res2_ss, True)
