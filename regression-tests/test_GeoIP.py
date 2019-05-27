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

#    def test_geoIP2City(self):
#        attrs = dict()
#        attrs['ip'] = '128.243.1.1'
#        r = self.customFuncWithName("geoip2", attrs)
#        j = r.json()
#        self.assertRegexpMatches(json.dumps(j), "Nottingham")
#        r.close()
    def test_geoIP2LookupVals(self):
        attrs = dict()
        attrs['ip'] = '62.31.28.109'
        r = self.customFuncWithName("geoip2_lookupValue", attrs)
        j = r.json()
        self.assertRegexpMatches(json.dumps(j['city']), "London")
        self.assertRegexpMatches(json.dumps(j['accuracy']), "200")
        self.assertRegexpMatches(json.dumps(j['latitude']), "51.4776")
        self.assertRegexpMatches(json.dumps(j['is_in_eu']), "1")
        r.close()
