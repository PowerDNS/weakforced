import requests
import socket
import time
import json
from test_helper import ApiTestCase

class TestGeoIP(ApiTestCase):

    def test_geoIP(self):
        # don't allow IPs from Japan (arbitrary)
        r = self.allowFunc('baddie', '112.78.112.20', "1234")
        j = r.json()
        self.assertEquals(j['status'], -1)
        self.assertRegexpMatches(json.dumps(j), "Japan")
        r.close()

    def test_geoIPCity(self):
        # Don't allow IPs from Wokingham
        r = self.allowFunc('baddie', '86.128.244.97', "1234")
        j = r.json()
        self.assertEquals(j['status'], -1)
        self.assertRegexpMatches(json.dumps(j), "Wokingham")
        r.close()

    def test_geoIP2City(self):
        attrs = dict()
        attrs['ip'] = '86.128.244.97'
        r = self.customFuncWithName("geoip2", attrs)
        j = r.json()
        self.assertRegexpMatches(json.dumps(j), "Wokingham")
        r.close()

    
