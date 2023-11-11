import requests
import socket
import time
import json
from test_helper import ApiTestCase

class TestAttrs(ApiTestCase):

    def test_Attrs(self):
        attrs = dict()
        attrs['accountStatus'] = "normal"
        r = self.allowFuncAttrs('fredbloggs', '127.0.0.1', "1234",  attrs)
        j = r.json()
        self.assertEqual(j['status'], 0)
        r.close()

        attrs['accountStatus'] = "blocked"        
        r = self.allowFuncAttrs('fredbloggs', '127.0.0.1', "1234", attrs)
        j = r.json()
        self.assertEqual(j['status'], -1)

        attrs['accountStatus'] = "normal"        
        attrs['countryList'] = [ "UK", "US" ]
        r = self.allowFuncAttrs('fredbloggs', '127.0.0.1', "1234", attrs)
        j = r.json()
        self.assertEqual(j['status'], 0)
        r.close()

        attrs['countryList'] = [ "UK", "US", "Blockmestan" ]
        r = self.allowFuncAttrs('fredbloggs', '127.0.0.1', "1234", attrs)
        j = r.json()
        self.assertEqual(j['status'], -1)

