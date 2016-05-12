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

        self.writeCmdToConsole("ca4 = newCA(\"192.168.72.14\")")
        self.writeCmdToConsole("ca6 = newCA(\"2001:503:ba3e::2:30\")")
        self.writeCmdToConsole("blacklistIP(ca4, 10, \"test blacklist\")")
        self.writeCmdToConsole("blacklistIP(ca6, 10, \"test blacklist\")")

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

        self.writeCmdToConsole("blacklistLogin(\"goodie\", 10, \"test blacklist\")")

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

        self.writeCmdToConsole("ca4 = newCA(\"192.168.72.14\")")
        self.writeCmdToConsole("blacklistIPLogin(ca4, \"goodie\", 10, \"test blacklist\")")

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
