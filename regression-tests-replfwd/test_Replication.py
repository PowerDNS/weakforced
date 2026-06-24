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
        r = self.allowFunc('ivbaddiereplication', '127.0.0.1', "1234")
        j = r.json()
        self.assertEqual(j['status'], 0)
        r.close()

        for i in range(22):
            r = self.reportFunc('ivbaddiereplication', '127.0.0.1', "1234'%s" % i, True)
            r.json()

        time.sleep(1)
        r = self.allowFuncReplica('ivbaddiereplication', '127.0.0.1', "1234")
        j = r.json()
        self.assertEqual(j['status'], -1)

        # Wait for the time windows to clear and then check again
        time.sleep(16)
        r = self.allowFuncReplica('ivbaddiereplication', '127.0.0.1', "1234")
        j = r.json()
        self.assertEqual(j['status'], 0)

    def test_FailedLogins(self):
        r = self.allowFunc('flbaddiereplication', '128.0.0.1', "1234")
        j = r.json()
        self.assertEqual(j['status'], 0)
        r.close()

        for i in range(32):
            r = self.reportFunc('flbaddiereplication', '128.0.0.1', "1234", False)
            r.json()

        time.sleep(1)
        r = self.allowFuncReplica('flbaddiereplication', '128.0.0.1', "1234")
        j = r.json()
        self.assertEqual(j['status'], -1)

        # Wait for the time windows to clear and then check again
        time.sleep(16)
        r = self.allowFuncReplica('flbaddiereplication', '128.0.0.1', "1234")
        j = r.json()
        self.assertEqual(j['status'], 0)

    def test_diffIPs(self):
        r = self.allowFunc('ipbaddiereplication', '227.0.0.1', "1234")
        j = r.json()
        self.assertEqual(j['status'], 0)
        r.close()

        for i in range(12):
            r = self.reportFunc('ipbaddiereplication', "227.0.0.%s" % i, "1234", True)
            r.json()

        time.sleep(1)
        r = self.allowFuncReplica('ipbaddiereplication', '227.0.0.1', "1234")
        j = r.json()
        self.assertEqual(j['status'], -1)

        # Wait for the time windows to clear and then check again
        time.sleep(16)
        r = self.allowFuncReplica('ipbaddiereplication', '227.0.0.1', "1234")
        j = r.json()
        self.assertEqual(j['status'], 0)


    def test_subTest(self):
        r = self.allowFunc('subbaddiereplication', '228.0.0.1', "1234")
        j = r.json()
        self.assertEqual(j['status'], 0)
        r.close()

        for i in range(42):
            r = self.reportFunc('subbaddiereplication', '228.0.0.1', "1234", False)
            r.json()

        time.sleep(1)
        r = self.allowFuncReplica('subbaddiereplication', '228.0.0.1', "1234")
        j = r.json()
        self.assertEqual(j['status'], -1)

        # Wait for the time windows to clear and then check again
        time.sleep(15)
        r = self.allowFuncReplica('subbaddiereplication', '227.0.0.1', "1234")
        j = r.json()
        self.assertEqual(j['status'], 0)

    def test_Prefixv4(self):
        r = self.allowFunc('ipv4baddiereplication', '114.31.193.200', "1234")
        j = r.json()
        self.assertEqual(j['status'], 0)
        r.close()

        for i in range(50):
            r = self.reportFunc('ipv4baddiereplication%s' % i, "114.31.193.%s" % i, "1234", True)
            r.json()

        time.sleep(1)
        r = self.allowFuncReplica('ipv4baddiereplication', '114.31.193.200', "1234")
        j = r.json()
        self.assertEqual(j['status'], -1)

        # Wait for the time windows to clear and then check again
        time.sleep(16)
        r = self.allowFuncReplica('ipv4baddiereplication', '114.31.193.200', "1234")
        j = r.json()
        self.assertEqual(j['status'], 0)

    def test_PrefixMappedv4(self):
        r = self.allowFunc('mappedipv4baddiereplication', '::ffff:114.31.193.200', "1234")
        j = r.json()
        self.assertEqual(j['status'], 0)
        r.close()

        for i in range(50):
            r = self.reportFunc('mappedipv4baddiereplication%s' % i, "::ffff:%s.31.193.200" % i, "1234", True)
            r.json()

        time.sleep(1)
        r = self.allowFuncReplica('mappedipv4baddiereplication', '::ffff:114.31.193.200', "1234")
        j = r.json()
        self.assertEqual(j['status'], 0)

        # Wait for the time windows to clear and then check again
        time.sleep(16)
        r = self.allowFuncReplica('mappedipv4baddiereplication', '::ffff:114.31.193.200', "1234")
        j = r.json()
        self.assertEqual(j['status'], 0)

    def test_Prefixv6(self):
        r = self.allowFunc('ipv6baddiereplication', '2001:c78::1000', "1234")
        j = r.json()
        self.assertEqual(j['status'], 0)
        r.close()

        for i in range(50):
            r = self.reportFunc('ipv6baddiereplication%s' % i, "2001:c78::%s" % i, "1234", True)
            r.json()

        time.sleep(1)
        r = self.allowFuncReplica('ipv6baddiereplication', '2001:c78::1000', "1234")
        j = r.json()
        self.assertEqual(j['status'], -1)

        # Wait for the time windows to clear and then check again
        time.sleep(16)
        r = self.allowFuncReplica('ipv6baddiereplication', '2001:c78::1000', "1234")
        j = r.json()
        self.assertEqual(j['status'], 0)

    def test_expiry(self):
        r = self.allowFunc('expirebaddiereplication', '132.0.0.1', "1234")
        j = r.json()
        self.assertEqual(j['status'], 0)
        r.close()

        for i in range(50):
            r = self.reportFunc('expirebaddiereplication%s' % i, "132.0.0.1", "1234", True)
            r.json()

        time.sleep(1)
        r = self.allowFuncReplica('expirebaddiereplication', '132.0.0.1', "1234")
        j = r.json()
        self.assertEqual(j['status'], -1)

        # Wait for the expiry thread to delete everything bigger than size (10) and then check again
        time.sleep(30)
        r = self.allowFuncReplica('expirebaddiereplication', '132.0.0.1', "1234")
        j = r.json()
        self.assertEqual(j['status'], 0)

    def test_Reset(self):
        r = self.allowFunc('resetbaddiereplication', '128.0.0.1', "1234")
        j = r.json()
        self.assertEqual(j['status'], 0)
        r.close()

        for i in range(32):
            r = self.reportFunc('resetbaddiereplication', '128.0.0.1', "1234", False)
            r.json()

        time.sleep(1)
        r = self.allowFuncReplica('resetbaddiereplication', '128.0.0.1', "1234")
        j = r.json()
        self.assertEqual(j['status'], -1)

        r = self.resetFunc('resetbaddiereplication', '128.0.0.1')
        j = r.json()
        self.assertEqual(j['status'], "ok")

        time.sleep(1)
        r = self.allowFuncReplica('resetbaddiereplication', '128.0.0.1', "1234")
        j = r.json()
        self.assertEqual(j['status'], 0)
        
    def test_resetField(self):
        r = self.incLogins("resetFieldTest")
        j = r.json()
        self.assertEqual(j['r_attrs']['countLogins'], '1')

        time.sleep(1)
        
        r = self.countLoginsReplica("resetFieldTest")
        j = r.json()
        self.assertEqual(j['r_attrs']['countLogins'], '1')

        r = self.resetLogins("resetFieldTest")
        j = r.json()
        self.assertEqual(j['r_attrs']['countLogins'], '0')

        time.sleep(1)

        r = self.countLoginsReplica("resetFieldTest")
        j = r.json()
        self.assertEqual(j['r_attrs']['countLogins'], '0')

    def test_BlackWhitelist(self):
        self.addBLEntryIP('12.12.12.12', 60, "replication bl test")
        self.addWLEntryIP('13.13.13.13', 60, "replication wl test")

        r = self.getBLFunc();
        j = r.json()
        self.assertNotEqual(str.find(json.dumps(j), '12.12.12.12'), -1)

        r = self.getWLFunc();
        j = r.json()
        self.assertNotEqual(str.find(json.dumps(j), '13.13.13.13'), -1)
        
        time.sleep(1)
        
        r = self.getBLFuncReplica();
        j = r.json()
        self.assertNotEqual(str.find(json.dumps(j), '12.12.12.12'), -1)

    def test_addRemoveSiblingsForwarders(self):
        self.assertTrue(self.removeSibling("127.0.0.1", 4001).ok)
        self.assertTrue(self.removeSiblingReplFwdDest("127.0.0.1", 8002).ok)
        self.assertTrue(self.removeSiblingReplFwdSrc("127.0.0.1").ok)
        self.assertTrue(self.addSibling("127.0.0.1", 4001, "udp", "qjShDn9gnCunXSH5quIXoNzrduomuTomWpvoePzi434=").ok)
        self.assertTrue(self.addSiblingReplFwdDest("127.0.0.1", 8002, "udp", "d6ZYYP/nSXQwfoSOgbcJSTE3zPe6Ty3KRRNixBfysA0=").ok)
        self.assertTrue(self.addSiblingReplFwdSrc("127.0.0.1").ok)
        self.assertTrue(self.addBLEntryIP('9.2.4.6', 60, "remove and add sibling test").ok)

        time.sleep(1)

        r = self.getBLFuncReplica()
        j = r.json()
        self.assertNotEqual(str.find(json.dumps(j), '9.2.4.6'), -1)

    def test_SetSiblingsForwarders(self):
        self.assertTrue(self.setSiblings({"siblings": [
            {"sibling_host": "127.0.0.1", "sibling_port": 4001, "sibling_protocol": "udp", "encryption_key": "qjShDn9gnCunXSH5quIXoNzrduomuTomWpvoePzi434="},
        ]}).ok)
        self.assertTrue(self.setSiblingsReplFwdSrc({"siblings": [
            {"sibling_host": "127.0.0.1"}
        ]}).ok)
        self.assertTrue(self.setSiblingReplFwdDests({"siblings": [
            {"sibling_host": "127.0.0.1", "sibling_port": 8002, "sibling_protocol": "udp", "encryption_key": "d6ZYYP/nSXQwfoSOgbcJSTE3zPe6Ty3KRRNixBfysA0="}
        ]}).ok)
        self.assertTrue(self.addBLEntryIP('10.2.4.6', 60, "remove and add sibling test").ok)

        time.sleep(1)

        r = self.getBLFuncReplica();
        j = r.json()
        self.assertNotEqual(str.find(json.dumps(j), '10.2.4.6'), -1)