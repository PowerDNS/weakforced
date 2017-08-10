import requests
import socket
import subprocess
import sys
import time
import json
from test_helper import ApiTestCase

class TestBlacklist(ApiTestCase):

    def test_NetmaskBlacklist(self):
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
        
        r = self.allowFunc('goodie', '193.168.72.14', "1234")
        j = r.json()
        self.assertEquals(j['status'], -1)
        r.close()

        r = self.allowFunc('goodie', '2002:503:ba3e::2:30', "1234")
        j = r.json()
        self.assertEquals(j['status'], -1)
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
    
    def test_IPBlacklist(self):
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
        
        r = self.allowFunc('goodie', '192.168.72.14', "1234")
        j = r.json()
        self.assertEquals(j['status'], -1)
        r.close()

        r = self.allowFunc('goodie', '2001:503:ba3e::2:30', "1234")
        j = r.json()
        self.assertEquals(j['status'], -1)
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
    
    def test_LoginBlacklist(self):
        r = self.allowFunc('goodie', '192.168.72.14', "1234")
        j = r.json()
        self.assertEquals(j['status'], 0)
        r.close()

        r = self.addBLEntryLogin("goodie", 10, "test blacklist")
        j = r.json()
        self.assertEquals(j['status'], 'ok')
        
        r = self.allowFunc('goodie', '192.168.72.14', "1234")
        j = r.json()
        self.assertEquals(j['status'], -1)
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
        
        r = self.allowFunc('goodie', '192.168.72.14', "1234")
        j = r.json()
        self.assertEquals(j['status'], -1)
        r.close()

        r = self.allowFunc('goody', '192.168.72.14', "1234")
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

    def test_PersistBlacklist(self):
        cmd3 = ("../wforce -C ./wforce3.conf -R ../regexes.yaml").split()
        proc3 = subprocess.Popen(cmd3, close_fds=True)
        time.sleep(1)
        
        r = self.addBLEntryIPPersist("99.99.99.99", 10, "test blacklist")
        j = r.json()
        self.assertEquals(j['status'], 'ok')

        print "Killing process"
        proc3.terminate()
        print "Waiting for process"
        proc3.wait()

        proc3 = subprocess.Popen(cmd3, close_fds=True)

        time.sleep(1)
        
        r = self.getBLFuncPersist()
        j = r.json()
        self.assertNotEqual(str.find(json.dumps(j),'99.99.99.99'), -1)

        proc3.terminate()
        proc3.wait()
