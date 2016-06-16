import requests
import socket
import time
import json
from test_helper import ApiTestCase

class TestTimeWindowsReplication(ApiTestCase):
    def setUp(self):
        time.sleep(16)
        super(TestTimeWindowsReplication, self).setUp()
    
    def test_invalidPasswords(self):
        r = self.allowFunc('ivbaddiereplication', '227.0.0.1', "1234")
        j = r.json()
        self.assertEquals(j['status'], 0)
        r.close()

        for i in range(25):
            r = self.reportFunc('ivbaddiereplication', '227.0.0.1', "1234'%s" % i, 'true')
            r.json()

        r = self.allowFuncReplica('ivbaddiereplication', '227.0.0.1', "1234")
        j = r.json()
        self.assertEquals(j['status'], -1)

        # Wait for the time windows to clear and then check again
        time.sleep(15)
        r = self.allowFuncReplica('ivbaddiereplication', '227.0.0.1', "1234")
        j = r.json()
        self.assertEquals(j['status'], 0)

    def test_FailedLogins(self):
        r = self.allowFunc('flbaddiereplication', '228.0.0.1', "1234")
        j = r.json()
        self.assertEquals(j['status'], 0)
        r.close()

        for i in range(31):
            r = self.reportFunc('flbaddiereplication', '228.0.0.1', "1234", 'false')
            r.json()

        r = self.allowFuncReplica('flbaddiereplication', '228.0.0.1', "1234")
        j = r.json()
        self.assertEquals(j['status'], -1)

        # Wait for the time windows to clear and then check again
        time.sleep(15)
        r = self.allowFuncReplica('flbaddiereplication', '228.0.0.1', "1234")
        j = r.json()
        self.assertEquals(j['status'], 0)

    def test_diffIPs(self):
        r = self.allowFunc('ipbaddiereplication', '227.0.0.1', "1234")
        j = r.json()
        self.assertEquals(j['status'], 0)
        r.close()

        for i in range(12):
            r = self.reportFunc('ipbaddiereplication', "227.0.0.%s" % i, "1234", 'true')
            r.json()

        r = self.allowFuncReplica('ipbaddiereplication', '227.0.0.1', "1234")
        j = r.json()
        self.assertEquals(j['status'], -1)

        # Wait for the time windows to clear and then check again
        time.sleep(15)
        r = self.allowFuncReplica('ipbaddiereplication', '227.0.0.1', "1234")
        j = r.json()
        self.assertEquals(j['status'], 0)

    def test_subTest(self):
        r = self.allowFunc('subbaddiereplication', '228.0.0.1', "1234")
        j = r.json()
        self.assertEquals(j['status'], 0)
        r.close()

        for i in range(41):
            r = self.reportFunc('subbaddiereplication', '228.0.0.1', "1234", 'false')
            r.json()

        r = self.allowFuncReplica('subbaddiereplication', '228.0.0.1', "1234")
        j = r.json()
        self.assertEquals(j['status'], -1)

        # Wait for the time windows to clear and then check again
        time.sleep(15)
        r = self.allowFuncReplica('subbaddiereplication', '227.0.0.1', "1234")
        j = r.json()
        self.assertEquals(j['status'], 0)

    def test_countMinPrefixv4(self):
        r = self.allowFunc('ipv4baddiereplication', '114.31.193.200', "1234")
        j = r.json()
        self.assertEquals(j['status'], 0)
        r.close()

        for i in range(12):
            r = self.reportFunc('ipv4baddiereplication%s' % i, "114.31.193.%s" % i, "1234", 'true')
            r.json()

        r = self.allowFuncReplica('ipv4baddiereplication', '114.31.193.200', "1234")
        j = r.json()
        self.assertEquals(j['status'], -1)

        # Wait for the time windows to clear and then check again
        time.sleep(15)
        r = self.allowFuncReplica('ipv4baddiereplication', '114.31.193.200', "1234")
        j = r.json()
        self.assertEquals(j['status'], 0)

    def test_countMinPrefixv6(self):
        r = self.allowFunc('ipv6baddiereplication', '2001:c78::1000', "1234")
        j = r.json()
        self.assertEquals(j['status'], 0)
        r.close()

        for i in range(12):
            r = self.reportFunc('ipv6baddiereplication%s' % i, "2001:c78::%s" % i, "1234", 'true')
            r.json()

        r = self.allowFuncReplica('ipv6baddiereplication', '2001:c78::1000', "1234")
        j = r.json()
        self.assertEquals(j['status'], -1)

        # Wait for the time windows to clear and then check again
        time.sleep(15)
        r = self.allowFuncReplica('ipv6baddiereplication', '2001:c78::1000', "1234")
        j = r.json()
        self.assertEquals(j['status'], 0)

    def test_expiry(self):
        r = self.allowFunc('expirebaddiereplication', '227.0.0.1', "1234")
        j = r.json()
        self.assertEquals(j['status'], 0)
        r.close()

        for i in range(40):
            r = self.reportFunc('expirebaddiereplication%s' % i, "227.0.0.1", "1234", 'true')
            r.json()

        r = self.allowFuncReplica('expirebaddiereplication', '227.0.0.1', "1234")
        j = r.json()
        self.assertEquals(j['status'], -1)

        # Wait for the expiry thread to delete everything bigger than size (10) and then check again
        time.sleep(3)
        r = self.allowFuncReplica('expirebaddiereplication', '227.0.01', "1234")
        j = r.json()
        self.assertEquals(j['status'], 0)

    def test_Reset(self):
        r = self.allowFunc('resetbaddiereplication', '228.0.0.1', "1234")
        j = r.json()
        self.assertEquals(j['status'], 0)
        r.close()

        for i in range(31):
            r = self.reportFunc('resetbaddiereplication', '228.0.0.1', "1234", 'false')
            r.json()

        r = self.allowFuncReplica('resetbaddiereplication', '228.0.0.1', "1234")
        j = r.json()
        self.assertEquals(j['status'], -1)

        r = self.resetFunc('resetbaddiereplication', '228.0.0.1')
        j = r.json()
        self.assertEquals(j['status'], "ok")

        r = self.allowFuncReplica('resetbaddiereplication', '228.0.0.1', "1234")
        j = r.json()
        self.assertEquals(j['status'], 0)
