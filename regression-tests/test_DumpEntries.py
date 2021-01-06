import requests
import socket
import time
import json
import os
from subprocess import call
from test_helper import ApiTestCase

class TestDumpEntries(ApiTestCase):

    def test_dumpentries(self):
        for i in range(100):
            r = self.reportFunc('dumpentries', '199.99.99.99', "1234'%s" % i, False)

        dumpfile = "/tmp/wfdb.txt"
        result = call(["../wforce/wf_dump_entries", "-f", dumpfile, "-u", "http://127.0.0.1:8084/", "-l", "127.0.0.1", "-p", "9999", "-w", os.environ.get('APIKEY', 'super')])
        if result:
            f = open(dumpfile, "r")
            found = False
            for x in f:
                if x.find("199.99.99.99"):
                    found = True
                    break

            self.assertTrue(found)
