import requests
import socket
import time
import json
from test_helper import ApiTestCase

class TestEightBit(ApiTestCase):
    def setUp(self):
        time.sleep(16)
        super(TestEightBit, self).setUp()

    def test8Bit(self):
        r = self.customFuncWithName("EightBitKey", {})
        j = r.json()
        print(json.dumps(j))

        r = self.customFuncReplicaWithName("EightBitKey", {})
        j = r.json()
        print(json.dumps(j))
        count = int(j["r_attrs"]["count"])

        self.assertEquals(count, 2)
