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

        for i in range(25):
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
        r = self.allowFunc('flbaddie', '128.0.0.1', "1234")
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

    def test_countMinPrefixv4(self):
        self.writeFileToConsole(configFile)
        r = self.allowFunc('ipv4baddie', '114.31.192.200', "1234")
        j = r.json()
        self.assertEquals(j['status'], 0)
        r.close()

        for i in range(12):
            r = self.reportFunc('ipv4baddie%s' % i, "114.31.192.%s" % i, "1234", 'true')
            r.json()

        r = self.allowFunc('ipv4baddie', '114.31.192.200', "1234")
        j = r.json()
        self.assertEquals(j['status'], -1)

        # Wait for the time windows to clear and then check again
        time.sleep(15)
        r = self.allowFunc('ipv4baddie', '114.31.192.200', "1234")
        j = r.json()
        self.assertEquals(j['status'], 0)

    def test_countMinPrefixv6(self):
        self.writeFileToConsole(configFile)
        r = self.allowFunc('ipv6baddie', '2001:c78::1000', "1234")
        j = r.json()
        self.assertEquals(j['status'], 0)
        r.close()

        for i in range(12):
            r = self.reportFunc('ipv6baddie%s' % i, "2001:c78::%s" % i, "1234", 'true')
            r.json()

        r = self.allowFunc('ipv6baddie', '2001:c78::1000', "1234")
        j = r.json()
        self.assertEquals(j['status'], -1)

        # Wait for the time windows to clear and then check again
        time.sleep(15)
        r = self.allowFunc('ipv6baddie', '2001:c78::1000', "1234")
        j = r.json()
        self.assertEquals(j['status'], 0)

    def test_expiry(self):
        self.writeFileToConsole(configFile)
        r = self.allowFunc('expirebaddie', '127.0.0.1', "1234")
        j = r.json()
        self.assertEquals(j['status'], 0)
        r.close()

        for i in range(30):
            r = self.reportFunc('expirebaddie%s' % i, "127.0.0.1", "1234", 'true')
            r.json()

        r = self.allowFunc('expirebaddie', '127.0.0.1', "1234")
        j = r.json()
        self.assertEquals(j['status'], -1)

        # Wait for the expiry thread to delete everything bigger than size (10) and then check again
        time.sleep(3)
        r = self.allowFunc('expirebaddie', '127.0.01', "1234")
        j = r.json()
        self.assertEquals(j['status'], 0)

    def test_Reset(self):
        self.writeFileToConsole(configFile)
        r = self.allowFunc('resetbaddie', '128.0.0.1', "1234")
        j = r.json()
        self.assertEquals(j['status'], 0)
        r.close()

        for i in range(31):
            r = self.reportFunc('resetbaddie', '128.0.0.1', "1234", 'false')
            r.json()

        r = self.allowFunc('resetbaddie', '128.0.0.1', "1234")
        j = r.json()
        self.assertEquals(j['status'], -1)

        r = self.resetFunc('resetbaddie', '128.0.0.1')
        j = r.json()
        self.assertEquals(j['status'], "ok")

        r = self.allowFunc('resetbaddie', '128.0.0.1', "1234")
        j = r.json()
        self.assertEquals(j['status'], 0)
