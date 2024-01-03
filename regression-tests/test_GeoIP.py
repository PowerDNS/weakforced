import requests
import socket
import time
import json
import os
from test_helper import ApiTestCase

class TestGeoIP(ApiTestCase):
    def setUp(self):
        super(TestGeoIP, self).setUp()
        
    def test_geoIP(self):
        # don't allow IPs from Japan (arbitrary)
        r = self.allowFunc('baddie', '112.78.112.20', "1234")
        j = r.json()
        self.assertEqual(j['status'], -1)
        self.assertRegex(json.dumps(j), "Japan")
        r.close()

    def test_geoIP2City(self):
        attrs = dict()
        attrs['ip'] = '128.243.1.1'
        r = self.customFuncWithName("geoip2", attrs)
        j = r.json()
        self.assertRegex(json.dumps(j), "Nottingham")
        r.close()

    def test_geoIP2LookupVals(self):
        attrs = dict()
        attrs['ip'] = '128.243.21.1'
        r = self.customFuncWithName("geoip2_lookupValue", attrs)
        j = r.json()
        print(json.dumps(j))
        self.assertRegex(json.dumps(j['r_attrs']['city']), "Nottingham")
        self.assertRegex(json.dumps(j['r_attrs']['latitude']), "52.9044")
        r.close()
