import requests
import socket
import subprocess
import sys
import time
import json
from test_helper import ApiTestCase

class TestWhitelist(ApiTestCase):

    def test_NetmaskWhitelist(self):
        r = self.allowFunc('goodie', '193.168.72.14', "1234")
        j = r.json()
        self.assertEquals(j['status'], 0)
        r.close()

        r = self.allowFunc('goodie', '2002:503:ba3e::2:30', "1234")
        j = r.json()
        self.assertEquals(j['status'], 0)
        r.close()

        r = self.addBLEntryNetmask("193.168.0.0/16", 10, "test blacklist")
        j = r.json()
        self.assertEquals(j['status'], 'ok')

        r = self.addBLEntryNetmask("2002:503:ba3e::/64", 10, "test blacklist")
        j = r.json()
        self.assertEquals(j['status'], 'ok')

        r = self.addWLEntryNetmask("193.168.0.0/16", 10, "test whitelist")
        j = r.json()
        self.assertEquals(j['status'], 'ok')

        r = self.addWLEntryNetmask("2002:503:ba3e::/64", 10, "test whitelist")
        j = r.json()
        self.assertEquals(j['status'], 'ok')
        
        r = self.allowFunc('goodie', '193.168.72.14', "1234")
        j = r.json()
        self.assertEquals(j['status'], 0)
        r.close()

        r = self.allowFunc('goodie', '2002:503:ba3e::2:30', "1234")
        j = r.json()
        self.assertEquals(j['status'], 0)
        r.close()

        time.sleep(11);

        r = self.allowFunc('goodie', '193.168.72.14', "1234")
        j = r.json()
        self.assertEquals(j['status'], 0)
        r.close()
        
        r = self.allowFunc('goodie', '2002:503:ba3e::2:30', "1234")
        j = r.json()
        self.assertEquals(j['status'], 0)
        r.close()        
    
    def test_IPWhitelist(self):
        r = self.allowFunc('goodie', '192.168.72.14', "1234")
        j = r.json()
        self.assertEquals(j['status'], 0)
        r.close()

        r = self.allowFunc('goodie', '2001:503:ba3e::2:30', "1234")
        j = r.json()
        self.assertEquals(j['status'], 0)
        r.close()

        r = self.addBLEntryIP("192.168.72.14", 10, "test blacklist")
        j = r.json()
        self.assertEquals(j['status'], 'ok')

        r = self.addBLEntryIP("2001:503:ba3e::2:30", 10, "test blacklist")
        j = r.json()
        self.assertEquals(j['status'], 'ok')
        
        r = self.addWLEntryIP("192.168.72.14", 10, "test whitelist")
        j = r.json()
        self.assertEquals(j['status'], 'ok')

        r = self.addWLEntryIP("2001:503:ba3e::2:30", 10, "test whitelist")
        j = r.json()
        self.assertEquals(j['status'], 'ok')

        r = self.allowFunc('goodie', '192.168.72.14', "1234")
        j = r.json()
        self.assertEquals(j['status'], 0)
        r.close()

        r = self.allowFunc('goodie', '2001:503:ba3e::2:30', "1234")
        j = r.json()
        self.assertEquals(j['status'], 0)
        r.close()

        time.sleep(11);

        r = self.allowFunc('goodie', '192.168.72.14', "1234")
        j = r.json()
        self.assertEquals(j['status'], 0)
        r.close()
        
        r = self.allowFunc('goodie', '2001:503:ba3e::2:30', "1234")
        j = r.json()
        self.assertEquals(j['status'], 0)
        r.close()
    
    def test_LoginWhitelist(self):
        r = self.allowFunc('goodie', '192.168.72.14', "1234")
        j = r.json()
        self.assertEquals(j['status'], 0)
        r.close()

        r = self.addBLEntryLogin("goodie", 10, "test blacklist")
        j = r.json()
        self.assertEquals(j['status'], 'ok')

        r = self.addWLEntryLogin("goodie", 10, "test whitelist")
        j = r.json()
        self.assertEquals(j['status'], 'ok')
        
        r = self.allowFunc('goodie', '192.168.72.14', "1234")
        j = r.json()
        self.assertEquals(j['status'], 0)
        r.close()

        time.sleep(11);

        r = self.allowFunc('goodie', '192.168.72.14', "1234")
        j = r.json()
        self.assertEquals(j['status'], 0)
        r.close()

    def test_IPLoginBlacklist(self):
        r = self.allowFunc('goodie', '192.168.72.14', "1234")
        j = r.json()
        self.assertEquals(j['status'], 0)
        r.close()

        r = self.addBLEntryIPLogin("192.168.72.14", "goodie", 10, "test blacklist")
        j = r.json()
        self.assertEquals(j['status'], 'ok')

        r = self.addWLEntryIPLogin("192.168.72.14", "goodie", 10, "test whitelist")
        j = r.json()
        self.assertEquals(j['status'], 'ok')

        r = self.allowFunc('goodie', '192.168.72.14', "1234")
        j = r.json()
        self.assertEquals(j['status'], 0)
        r.close()

        r = self.allowFunc('goodie', '192.168.72.15', "1234")
        j = r.json()
        self.assertEquals(j['status'], 0)
        r.close()

        time.sleep(11);

        r = self.allowFunc('goodie', '192.168.72.14', "1234")
        j = r.json()
        self.assertEquals(j['status'], 0)
        r.close()

    def test_PersistWhitelist(self):
        cmd3 = ("../wforce/wforce -C ./wforce3.conf -R ../wforce/regexes.yaml").split()
        proc3 = subprocess.Popen(cmd3, close_fds=True)
        time.sleep(1)
        
        r = self.addWLEntryIPPersist("99.99.99.99", 10, "test whitelist")
        j = r.json()
        self.assertEquals(j['status'], 'ok')
        
        print("Killing process")
        proc3.terminate()
        print("Waiting for process")
        proc3.wait()

        proc3 = subprocess.Popen(cmd3, close_fds=True)

        time.sleep(1)
        
        r = self.getWLFuncPersist()
        j = r.json()
        self.assertNotEqual(str.find(json.dumps(j),'99.99.99.99'), -1)

        proc3.terminate()
        proc3.wait()
