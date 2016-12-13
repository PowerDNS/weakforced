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
        r.close()


        
