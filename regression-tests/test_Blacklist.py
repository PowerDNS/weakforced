import requests
import socket
import time
import json
from test_helper import ApiTestCase

class TestBlacklist(ApiTestCase):

    def test_IPBlacklist(self):
        r = self.allowFunc('goodie', '192.168.72.14', "1234")
        j = r.json()
        self.assertEquals(j['status'], 0)
        r.close()

        r = self.allowFunc('goodie', '2001:503:ba3e::2:30', "1234")
        j = r.json()
        self.assertEquals(j['status'], 0)
        r.close()

        r = self.addBLEntryIP("192.168.72.14", 10, "test blacklist");
        j = r.json()
        self.assertEquals(j['status'], 'ok')

        r = self.addBLEntryIP("2001:503:ba3e::2:30", 10, "test blacklist");
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

        r = self.addBLEntryLogin("goodie", 10, "test blacklist");
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

        r = self.addBLEntryIPLogin("192.168.72.14", "goodie", 10, "test blacklist");
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
