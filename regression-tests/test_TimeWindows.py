import requests
import socket
import time
import json
from test_helper import ApiTestCase

configFile = "./wforce-tw.conf"

class TestTimeWindows(ApiTestCase):

    def test_invalidPasswords(self):
        self.writeFileToConsole(configFile)
        r = self.allowFunc('ivbaddie', '127.0.0.1', "1234")
        j = r.json()
        self.assertEquals(j['status'], 0)
        r.close()

        for i in range(21):
            r = self.reportFunc('ivbaddie', '127.0.0.1', "1234'%s" % i, 'true')
            r.json()

        r = self.allowFunc('ivbaddie', '127.0.0.1', "1234")
        j = r.json()
        self.assertEquals(j['status'], -1)

        # Wait for the time windows to clear and then check again
        time.sleep(15)
        r = self.allowFunc('ivbaddie', '127.0.0.1', "1234")
        j = r.json()
        self.assertEquals(j['status'], 0)

    def test_FailedLogins(self):
        self.writeFileToConsole(configFile)
        r = self.allowFunc('flbaddie', '128.0.0.1', "1234")
        j = r.json()
        self.assertEquals(j['status'], 0)
        r.close()

        for i in range(31):
            r = self.reportFunc('flbaddie', '128.0.0.1', "1234", 'false')
            r.json()

        r = self.allowFunc('flbaddie', '128.0.0.1', "1234")
        j = r.json()
        self.assertEquals(j['status'], -1)

        # Wait for the time windows to clear and then check again
        time.sleep(15)
        r = self.allowFunc('flbaddie', '127.0.0.1', "1234")
        j = r.json()
        self.assertEquals(j['status'], 0)

    def test_diffIPs(self):
        self.writeFileToConsole(configFile)
        r = self.allowFunc('ipbaddie', '127.0.0.1', "1234")
        j = r.json()
        self.assertEquals(j['status'], 0)
        r.close()

        for i in range(12):
            r = self.reportFunc('ipbaddie', "127.0.0.%s" % i, "1234", 'true')
            r.json()

        r = self.allowFunc('ipbaddie', '127.0.0.1', "1234")
        j = r.json()
        self.assertEquals(j['status'], -1)

        # Wait for the time windows to clear and then check again
        time.sleep(15)
        r = self.allowFunc('ipbaddie', '127.0.0.1', "1234")
        j = r.json()
        self.assertEquals(j['status'], 0)

    def test_subTest(self):
        self.writeFileToConsole(configFile)
        r = self.allowFunc('subbaddie', '128.0.0.1', "1234")
        j = r.json()
        self.assertEquals(j['status'], 0)
        r.close()

        for i in range(41):
            r = self.reportFunc('subbaddie', '128.0.0.1', "1234", 'false')
            r.json()

        r = self.allowFunc('subbaddie', '128.0.0.1', "1234")
        j = r.json()
        self.assertEquals(j['status'], -1)

        # Wait for the time windows to clear and then check again
        time.sleep(15)
        r = self.allowFunc('subbaddie', '127.0.0.1', "1234")
        j = r.json()
        self.assertEquals(j['status'], 0)
        
