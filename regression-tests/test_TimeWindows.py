import requests
import socket
import time
import json
from test_helper import ApiTestCase

class TestTimeWindows(ApiTestCase):
    def setUp(self):
        time.sleep(16)
        super(TestTimeWindows, self).setUp()
        
    def test_invalidPasswords(self):
        r = self.allowFunc('ivbaddie', '127.0.0.1', "1234")
        j = r.json()
        self.assertEquals(j['status'], 0)
        r.close()

        for i in range(25):
            r = self.reportFunc('ivbaddie', '127.0.0.1', "1234'%s" % i, True)
            r.json()

        r = self.allowFunc('ivbaddie', '127.0.0.1', "1234")
        j = r.json()
        self.assertEquals(j['status'], -1)

        # Wait for the time windows to clear and then check again
        time.sleep(20)
        r = self.allowFunc('ivbaddie', '127.0.0.1', "1234")
        j = r.json()
        self.assertEquals(j['status'], 0)

    def test_FailedLogins(self):
        r = self.allowFunc('flbaddie', '128.0.0.1', "1234")
        j = r.json()
        self.assertEquals(j['status'], 0)
        r.close()

        for i in range(32):
            r = self.reportFunc('flbaddie', '128.0.0.1', "1234", False)
            r.json()

        r = self.allowFunc('flbaddie', '128.0.0.1', "1234")
        j = r.json()
        self.assertEquals(j['status'], -1)

        # Wait for the time windows to clear and then check again
        time.sleep(20)
        r = self.allowFunc('flbaddie', '128.0.0.1', "1234")
        j = r.json()
        self.assertEquals(j['status'], 0)

    def test_diffIPs(self):
        r = self.allowFunc('ipbaddie', '127.0.0.1', "1234")
        j = r.json()
        self.assertEquals(j['status'], 0)
        r.close()

        for i in range(12):
            r = self.reportFunc('ipbaddie', "127.0.0.%s" % i, "1234", True)
            r.json()

        r = self.allowFunc('ipbaddie', '127.0.0.1', "1234")
        j = r.json()
        self.assertEquals(j['status'], -1)

        # Wait for the time windows to clear and then check again
        time.sleep(20)
        r = self.allowFunc('ipbaddie', '127.0.0.1', "1234")
        j = r.json()
        self.assertEquals(j['status'], 0)

    def test_subTest(self):
        r = self.allowFunc('subbaddie', '128.0.0.1', "1234")
        j = r.json()
        self.assertEquals(j['status'], 0)
        r.close()

        for i in range(42):
            r = self.reportFunc('subbaddie', '128.0.0.1', "1234", False)
            r.json()

        r = self.allowFunc('subbaddie', '128.0.0.1', "1234")
        j = r.json()
        self.assertEquals(j['status'], -1)

        # Wait for the time windows to clear and then check again
        time.sleep(20)
        r = self.allowFunc('subbaddie', '127.0.0.1', "1234")
        j = r.json()
        self.assertEquals(j['status'], 0)

    def test_Prefixv4(self):
        r = self.allowFunc('ipv4baddie', '114.31.192.200', "1234")
        j = r.json()
        self.assertEquals(j['status'], 0)
        r.close()

        for i in range(50):
            r = self.reportFunc('ipv4baddie%s' % i, "114.31.192.%s" % i, "1234", True)
            r.json()

        r = self.allowFunc('ipv4baddie', '114.31.192.200', "1234")
        j = r.json()
        self.assertEquals(j['status'], -1)

        # Wait for the time windows to clear and then check again
        time.sleep(20)
        r = self.allowFunc('ipv4baddie', '114.31.192.200', "1234")
        j = r.json()
        self.assertEquals(j['status'], 0)

    def test_PrefixMappedv4(self):
        r = self.allowFunc('mappedipv4baddie', '::ffff:114.31.192.200', "1234")
        j = r.json()
        self.assertEquals(j['status'], 0)
        r.close()

        for i in range(50):
            r = self.reportFunc('mappedipv4baddie%s' % i, "::ffff:%s.31.192.200" % i, "1234", True)
            r.json()

        r = self.allowFunc('mappedipv4baddie', '::ffff:114.31.192.200', "1234")
        j = r.json()
        self.assertEquals(j['status'], 0)

        # Wait for the time windows to clear and then check again
        time.sleep(20)
        r = self.allowFunc('mappedipv4baddie', '::ffff:114.31.192.200', "1234")
        j = r.json()
        self.assertEquals(j['status'], 0)

    def test_Prefixv6(self):
        r = self.allowFunc('ipv6baddie', '2001:c78::1000', "1234")
        j = r.json()
        self.assertEquals(j['status'], 0)
        r.close()

        for i in range(50):
            r = self.reportFunc('ipv6baddie%s' % i, "2001:c78::%s" % i, "1234", True)
            r.json()

        r = self.allowFunc('ipv6baddie', '2001:c78::1000', "1234")
        j = r.json()
        self.assertEquals(j['status'], -1)

        # Wait for the time windows to clear and then check again
        time.sleep(20)
        r = self.allowFunc('ipv6baddie', '2001:c78::1000', "1234")
        j = r.json()
        self.assertEquals(j['status'], 0)

    def test_expiry(self):
        r = self.allowFunc('expirebaddie', '127.0.0.1', "1234")
        j = r.json()
        self.assertEquals(j['status'], 0)
        r.close()

        for i in range(20):
            r = self.reportFunc('expirebaddie%s' % i, "127.0.0.1", "1234", True)
            r.json()
        for i in range(20):
            r = self.reportFunc('expirebaddie%s' % i, "127.0.0.1", "1234", True)
            r.json()

        r = self.allowFunc('expirebaddie', '127.0.0.1', "1234")
        j = r.json()
        self.assertEquals(j['status'], -1)

        # Wait for the expiry thread to delete everything bigger than size (10) and then check again
        time.sleep(35)
        r = self.allowFunc('expirebaddie', '127.0.01', "1234")
        j = r.json()
        self.assertEquals(j['status'], 0)

    def test_Reset(self):
        time.sleep(16)
        r = self.allowFunc('resetbaddie', '128.0.0.1', "1234")
        j = r.json()
        self.assertEquals(j['status'], 0)
        r.close()

        for i in range(32):
            r = self.reportFunc('resetbaddie', '128.0.0.1', "1234", False)
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

    def test_ResetField(self):
        r = self.resetLogins("resetFieldTest")
        j = r.json()
        self.assertEquals(j['r_attrs']['countLogins'], '0')

        r = self.incLogins("resetFieldTest")
        j = r.json()
        self.assertEquals(j['r_attrs']['countLogins'], '1')

        r = self.resetLogins("resetFieldTest")
        j = r.json()
        self.assertEquals(j['r_attrs']['countLogins'], '0')

        r = self.countLogins("resetFieldTest")
        j = r.json()
        self.assertEquals(j['r_attrs']['countLogins'], '0')
