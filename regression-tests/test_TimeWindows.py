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
        time.sleep(15)
        r = self.allowFunc('ivbaddie', '127.0.0.1', "1234")
        j = r.json()
        self.assertEquals(j['status'], 0)

    def test_FailedLogins(self):
        r = self.allowFunc('flbaddie', '128.0.0.1', "1234")
        j = r.json()
        self.assertEquals(j['status'], 0)
        r.close()

<<<<<<< 81faf741ea8204ae7b293964eabf07698fc5255c
        for i in range(31):
            r = self.reportFunc('flbaddie', '128.0.0.1', "1234", False)
=======
        for i in range(32):
            r = self.reportFunc('flbaddie', '128.0.0.1', "1234", 'false')
>>>>>>> fix replication decryption
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
        time.sleep(15)
        r = self.allowFunc('ipbaddie', '127.0.0.1', "1234")
        j = r.json()
        self.assertEquals(j['status'], 0)

    def test_subTest(self):
        r = self.allowFunc('subbaddie', '128.0.0.1', "1234")
        j = r.json()
        self.assertEquals(j['status'], 0)
        r.close()

        for i in range(42):
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
        r = self.allowFunc('ipv4baddie', '114.31.192.200', "1234")
        j = r.json()
        self.assertEquals(j['status'], 0)
        r.close()

<<<<<<< 73a1b5557451e1c9f695a80ad8c23dcd819dbecb
        for i in range(12):
            r = self.reportFunc('ipv4baddie%s' % i, "114.31.192.%s" % i, "1234", True)
=======
        for i in range(50):
            r = self.reportFunc('ipv4baddie%s' % i, "114.31.192.%s" % i, "1234", 'true')
>>>>>>> regression for replication
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
        r = self.allowFunc('ipv6baddie', '2001:c78::1000', "1234")
        j = r.json()
        self.assertEquals(j['status'], 0)
        r.close()

<<<<<<< 73a1b5557451e1c9f695a80ad8c23dcd819dbecb
        for i in range(12):
            r = self.reportFunc('ipv6baddie%s' % i, "2001:c78::%s" % i, "1234", True)
=======
        for i in range(50):
            r = self.reportFunc('ipv6baddie%s' % i, "2001:c78::%s" % i, "1234", 'true')
>>>>>>> regression for replication
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
        r = self.allowFunc('expirebaddie', '127.0.0.1', "1234")
        j = r.json()
        self.assertEquals(j['status'], 0)
        r.close()

        for i in range(40):
            r = self.reportFunc('expirebaddie%s' % i, "127.0.0.1", "1234", True)
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
        time.sleep(16)
        r = self.allowFunc('resetbaddie', '128.0.0.1', "1234")
        j = r.json()
        self.assertEquals(j['status'], 0)
        r.close()

<<<<<<< 81faf741ea8204ae7b293964eabf07698fc5255c
        for i in range(31):
            r = self.reportFunc('resetbaddie', '128.0.0.1', "1234", False)
=======
        for i in range(32):
            r = self.reportFunc('resetbaddie', '128.0.0.1', "1234", 'false')
>>>>>>> fix replication decryption
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
