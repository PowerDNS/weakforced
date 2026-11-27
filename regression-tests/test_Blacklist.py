import requests
import socket
import sys
import time
import json
from mimesis import Internet 
from test_helper import ApiTestCase
from test_helper import running_process
from test_helper import with_prefix_config_server

class TestBlacklist(ApiTestCase):

    def test_NetmaskBlacklist(self):
        r = self.allowFunc('goodie', '193.168.72.14', "1234")
        j = r.json()
        self.assertEqual(j['status'], 0)
        r.close()

        r = self.allowFunc('goodie', '2002:503:ba3e::2:30', "1234")
        j = r.json()
        self.assertEqual(j['status'], 0)
        r.close()

        r = self.addBLEntryNetmask("193.168.0.0/16", 10, "test blacklist")
        j = r.json()
        self.assertEqual(j['status'], 'ok')

        r = self.addBLEntryNetmask("2002:503:ba3e::/64", 10, "test blacklist")
        j = r.json()
        self.assertEqual(j['status'], 'ok')
        
        r = self.allowFunc('goodie', '193.168.72.14', "1234")
        j = r.json()
        self.assertEqual(j['status'], -1)
        r.close()

        r = self.allowFunc('goodie', '2002:503:ba3e::2:30', "1234")
        j = r.json()
        self.assertEqual(j['status'], -1)
        r.close()

        time.sleep(11)

        r = self.allowFunc('goodie', '193.168.72.14', "1234")
        j = r.json()
        self.assertEqual(j['status'], 0)
        r.close()
        
        r = self.allowFunc('goodie', '2002:503:ba3e::2:30', "1234")
        j = r.json()
        self.assertEqual(j['status'], 0)
        r.close()        
    
    def test_IPBlacklist(self):
        r = self.allowFunc('goodie', '192.168.72.14', "1234")
        j = r.json()
        self.assertEqual(j['status'], 0)
        r.close()

        r = self.allowFunc('goodie', '2001:503:ba3e::2:30', "1234")
        j = r.json()
        self.assertEqual(j['status'], 0)
        r.close()

        r = self.addBLEntryIP("192.168.72.14", 10, "test blacklist")
        j = r.json()
        self.assertEqual(j['status'], 'ok')

        r = self.addBLEntryIP("2001:503:ba3e::2:30", 10, "test blacklist")
        j = r.json()
        self.assertEqual(j['status'], 'ok')

        r = self.allowFunc('goodie', '192.168.72.14', "1234")
        j = r.json()
        self.assertEqual(j['status'], -1)
        r.close()

        r = self.allowFunc('goodie', '2001:503:ba3e::2:30', "1234")
        j = r.json()
        self.assertEqual(j['status'], -1)
        r.close()

        time.sleep(11)

        r = self.allowFunc('goodie', '192.168.72.14', "1234")
        j = r.json()
        self.assertEqual(j['status'], 0)
        r.close()

        r = self.allowFunc('goodie', '2001:503:ba3e::2:30', "1234")
        j = r.json()
        self.assertEqual(j['status'], 0)
        r.close()

    @with_prefix_config_server
    def test_IPBlacklistDefaultPrefix(self):
        # With wforce6's /24 IPv4 and /64 IPv6 blacklist prefixes, IP-only
        # blacklist entries should match other addresses in the configured
        # prefix but not addresses outside that prefix.
        r = self.addBLEntryIPPrefixConfig("192.0.2.14", 60, "test prefix blacklist")
        j = r.json()
        self.assertEqual(j['status'], 'ok')

        r = self.allowFuncPrefixConfig('prefixblip', '192.0.2.99', "1234")
        j = r.json()
        self.assertEqual(j['status'], -1)
        r.close()

        r = self.allowFuncPrefixConfig('prefixblip', '192.0.3.14', "1234")
        j = r.json()
        self.assertEqual(j['status'], 0)
        r.close()

        r = self.addBLEntryIPPrefixConfig("2001:db8:1234:5678::1", 60, "test prefix blacklist")
        j = r.json()
        self.assertEqual(j['status'], 'ok')

        r = self.allowFuncPrefixConfig('prefixblipv6', '2001:db8:1234:5678::abcd', "1234")
        j = r.json()
        self.assertEqual(j['status'], -1)
        r.close()

        r = self.allowFuncPrefixConfig('prefixblipv6', '2001:db8:1234:5679::1', "1234")
        j = r.json()
        self.assertEqual(j['status'], 0)
        r.close()

    def test_LoginBlacklist(self):
        r = self.allowFunc('goodie', '192.168.72.14', "1234")
        j = r.json()
        self.assertEqual(j['status'], 0)
        r.close()

        r = self.addBLEntryLogin("goodie", 10, "test blacklist")
        j = r.json()
        self.assertEqual(j['status'], 'ok')
        
        r = self.allowFunc('goodie', '192.168.72.14', "1234")
        j = r.json()
        self.assertEqual(j['status'], -1)
        r.close()

        time.sleep(11)

        r = self.allowFunc('goodie', '192.168.72.14', "1234")
        j = r.json()
        self.assertEqual(j['status'], 0)
        r.close()

    def test_IPLoginBlacklist(self):
        r = self.allowFunc('goodie', '192.168.72.14', "1234")
        j = r.json()
        self.assertEqual(j['status'], 0)
        r.close()

        r = self.addBLEntryIPLogin("192.168.72.14", "goodie", 10, "test blacklist")
        j = r.json()
        self.assertEqual(j['status'], 'ok')
        
        r = self.allowFunc('goodie', '192.168.72.14', "1234")
        j = r.json()
        self.assertEqual(j['status'], -1)
        r.close()

        r = self.allowFunc('goody', '192.168.72.14', "1234")
        j = r.json()
        self.assertEqual(j['status'], 0)
        r.close()

    @with_prefix_config_server
    def test_IPLoginBlacklistDefaultPrefix(self):
        # Non-explicit blacklistIPLogin() should build its key using the
        # configured default IP prefix, while still requiring the login to
        # match exactly.
        attrs = {
            "ip": "192.0.4.14",
            "same_prefix_ip": "192.0.4.99",
            "different_prefix_ip": "192.0.5.14",
            "login": "prefixbl-login@foobar.com",
            "other_login": "prefixbl-other-login@foobar.com"
        }
        r = self.customFuncPrefixConfigWithName("AddPrefixBlacklistIPLogin", attrs)
        j = r.json()
        self.assertEqual(j['r_attrs']['status'], 'ok')

        r = self.allowFuncPrefixConfig('prefixbl-login', '192.0.4.99', "1234")
        j = r.json()
        self.assertEqual(j['status'], -1)
        r.close()

        r = self.allowFuncPrefixConfig('prefixbl-other-login', '192.0.4.99', "1234")
        j = r.json()
        self.assertEqual(j['status'], 0)
        r.close()

        r = self.allowFuncPrefixConfig('prefixbl-login', '192.0.5.14', "1234")
        j = r.json()
        self.assertEqual(j['status'], 0)
        r.close()

        r = self.customFuncPrefixConfigWithName("DelPrefixBlacklistIPLogin", attrs)
        j = r.json()
        self.assertEqual(j['r_attrs']['status'], 'ok')

    def test_NetmaskLoginBlacklist(self):
        # The HTTP API does not support prefix tuple entries because the built-in
        # allow path has no request-time prefix parameter to check them with.
        r = self.addBLEntryNetmaskLogin("198.51.104.0/24", "netmask-login-bl", 60, "test netmask login blacklist")
        j = r.json()
        self.assertEqual(j['status'], 'failure')
        self.assertIn('netmask is mutually exclusive with login and ja3', j['reason'])
        r.close()

    def test_IPNetmaskBlacklistConflict(self):
        payload = {
            'ip': '198.51.104.25',
            'netmask': '198.51.104.0/24',
            'login': 'netmask-login-bl',
            'expire_secs': 60,
            'reason': 'test netmask conflict'
        }
        r = self.session.post(
            self.url("/?command=addBLEntry"),
            data=json.dumps(payload),
            headers={'Content-Type': 'application/json'})
        j = r.json()
        self.assertEqual(j['status'], 'failure')
        self.assertIn('ip and netmask are mutually exclusive', j['reason'])
        r.close()

    def test_PersistBlacklist(self):
        cmd3 = ("../wforce/wforce -D -C ./wforce3.conf -R ../wforce/regexes.yaml").split()
        persistent_ja3 = "persist-ja3-blacklist"
        persistent_ipja3_ip = "198.51.120.14"
        persistent_ipja3 = "persist-ipja3-blacklist"
        # Long enough to outlive adding 2000 entries and restarting, also on a slow machine
        expire_secs = 300
        added_ips = set()
        with running_process(cmd3, close_fds=True):
            time.sleep(1)

            for i in range(2000):
                random_ip = Internet().ip_v4()
                r = self.addBLEntryIPPersist(random_ip, expire_secs, "test blacklist")
                j = r.json()
                self.assertEqual(j['status'], 'ok')
                added_ips.add(random_ip + "/32")
            r = self.addBLEntryJA3Persist(persistent_ja3, expire_secs, "test persistent ja3 blacklist")
            j = r.json()
            self.assertEqual(j['status'], 'ok')
            r = self.addBLEntryIPJA3Persist(persistent_ipja3_ip, persistent_ipja3, expire_secs, "test persistent ipja3 blacklist")
            j = r.json()
            self.assertEqual(j['status'], 'ok')

        with running_process(cmd3, close_fds=True):
            time.sleep(1)

            r = self.getBLFuncPersist()
            j = r.json()
            # Redis may still hold entries from an earlier run, so check that ours were all loaded back
            ip_entries = {entry['ip'] for entry in j['bl_entries'] if 'ip' in entry}
            self.assertEqual(added_ips - ip_entries, set())
            ja3_entries = {entry['ja3'] for entry in j['bl_entries'] if 'ja3' in entry}
            ipja3_entries = {entry['ipja3'] for entry in j['bl_entries'] if 'ipja3' in entry}
            self.assertIn(persistent_ja3, ja3_entries)
            self.assertIn(persistent_ipja3_ip + "/32:" + persistent_ipja3, ipja3_entries)

    def test_JA3Blacklist(self):
        r = self.allowFuncAttrs('ja3goodie', '192.168.49.14', "1234", {"ja3":"03456"})
        j = r.json()
        self.assertEqual(j['status'], 0)
        r.close()

        r = self.addBLEntryJA3("03456", 10, "test ja3 blacklist")
        j = r.json()
        self.assertEqual(j['status'], 'ok')

        r = self.allowFuncAttrs('ja3goodie', '192.168.49.14', "1234", {"ja3":"03456"})
        j = r.json()
        self.assertEqual(j['status'], -1)
        r.close()

        time.sleep(11)

        r = self.allowFuncAttrs('ja3goodie', '192.168.49.14', "1234", {"ja3":"03456"})
        j = r.json()
        self.assertEqual(j['status'], 0)
        r.close()

    def test_JA3BlacklistReset(self):
        r = self.allowFuncAttrs('ja3goodie', '192.168.49.14', "1234", {"ja3":"034567"})
        j = r.json()
        self.assertEqual(j['status'], 0)
        r.close()

        r = self.addBLEntryJA3("034567", 3600, "test ja3 blacklist")
        j = r.json()
        self.assertEqual(j['status'], 'ok')

        r = self.allowFuncAttrs('ja3goodie', '192.168.49.14', "1234", {"ja3":"034567"})
        j = r.json()
        self.assertEqual(j['status'], -1)
        r.close()

        time.sleep(11)

        r= self.resetJA3Func("034567", None)

        r = self.allowFuncAttrs('ja3goodie', '192.168.49.14', "1234", {"ja3":"034567"})
        j = r.json()
        self.assertEqual(j['status'], 0)
        r.close()
    def test_IPJA3Blacklist(self):
        r = self.allowFuncAttrs('ja3goodie', '192.168.41.14', "1234", {"ja3":"03456"})
        j = r.json()
        self.assertEqual(j['status'], 0)
        r.close()

        r = self.addBLEntryIPJA3("192.168.41.14", "03456", 10, "test ipja3 blacklist")
        j = r.json()
        self.assertEqual(j['status'], 'ok')

        r = self.allowFuncAttrs('ja3goodie', '192.168.41.14', "1234", {"ja3":"03456"})
        j = r.json()
        self.assertEqual(j['status'], -1)
        r.close()

        r = self.allowFuncAttrs('ja3goody', '192.168.41.15', "1234", {"ja3":"03456"})
        j = r.json()
        self.assertEqual(j['status'], 0)
        r.close()

    @with_prefix_config_server
    def test_IPJA3BlacklistDefaultPrefix(self):
        # Non-explicit blacklistIPJA3() should build its key using the
        # configured default IP prefix, while still requiring the JA3 value to
        # match exactly.
        attrs = {
            "ip": "192.0.6.14",
            "same_prefix_ip": "192.0.6.99",
            "different_prefix_ip": "192.0.7.14",
            "ja3": "prefixbl-ja3",
            "other_ja3": "prefixbl-other-ja3"
        }
        r = self.customFuncPrefixConfigWithName("AddPrefixBlacklistIPJA3", attrs)
        j = r.json()
        self.assertEqual(j['r_attrs']['status'], 'ok')

        r = self.allowFuncPrefixConfigAttrs('prefixbl-ja3-user', '192.0.6.99', "1234", {"ja3":"prefixbl-ja3"})
        j = r.json()
        self.assertEqual(j['status'], -1)
        r.close()

        r = self.allowFuncPrefixConfigAttrs('prefixbl-ja3-user', '192.0.6.99', "1234", {"ja3":"prefixbl-other-ja3"})
        j = r.json()
        self.assertEqual(j['status'], 0)
        r.close()

        r = self.allowFuncPrefixConfigAttrs('prefixbl-ja3-user', '192.0.7.14', "1234", {"ja3":"prefixbl-ja3"})
        j = r.json()
        self.assertEqual(j['status'], 0)
        r.close()

        r = self.customFuncPrefixConfigWithName("DelPrefixBlacklistIPJA3", attrs)
        j = r.json()
        self.assertEqual(j['r_attrs']['status'], 'ok')

    def test_NetmaskJA3Blacklist(self):
        # Explicit prefix IP/JA3 entries are available from Lua, not the REST API.
        r = self.addBLEntryNetmaskJA3("198.51.106.0/24", "netmask-ja3-bl", 60, "test netmask ja3 blacklist")
        j = r.json()
        self.assertEqual(j['status'], 'failure')
        self.assertIn('netmask is mutually exclusive with login and ja3', j['reason'])
        r.close()

    def test_ExplicitPrefixBlacklistFunctions(self):
        # Explicit-prefix blacklist Lua functions should honor the supplied
        # prefix even though the main regression instance keeps exact-IP
        # defaults.
        attrs = {
            "ip": "198.51.100.14",
            "same_prefix_ip": "198.51.100.99",
            "different_prefix_ip": "198.51.101.14",
            "login": "explicitbl-login",
            "ja3": "explicitbl-ja3",
            "prefix": "24"
        }
        r = self.customFuncWithName("ExplicitPrefixBlacklist", attrs)
        j = r.json()
        self.assertEqual(j['r_attrs']['login_same_prefix'], 'true')
        self.assertEqual(j['r_attrs']['login_different_prefix'], 'false')
        self.assertEqual(j['r_attrs']['ja3_same_prefix'], 'true')
        self.assertEqual(j['r_attrs']['ja3_different_prefix'], 'false')

        r = self.allowFuncAttrs('ja3goodie', '192.168.41.14', "1234", {"ja3":"111111"})
        j = r.json()
        self.assertEqual(j['status'], 0)
        r.close()

        time.sleep(11)

        r = self.allowFuncAttrs('ja3goodie', '192.168.41.14', "1234", {"ja3":"03456"})
        j = r.json()
        self.assertEqual(j['status'], 0)
        r.close()
