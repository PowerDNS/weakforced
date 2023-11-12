import requests
import socket
import time
import json
from test_helper import ApiTestCase

class TestBlock(ApiTestCase):

    def test_block(self):
        r = self.allowFunc("baddie", "127.0.0.1", "1234")
        j = r.json()
        self.assertEqual(j['status'], 0)
        r.close()

        for i in range(100):
            r = self.reportFunc('baddie', '127.0.0.1', "1234'%s" % i, False)
            r.json()

        r = self.allowFunc('baddie', '127.0.0.1', "1234")
        j = r.json()
        self.assertEqual(j['status'], -1)
        r.close()
        
