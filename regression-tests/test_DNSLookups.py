import requests
import socket
import time
import json
from test_helper import ApiTestCase

class TestDNSLookups(ApiTestCase):

    def test_DNSLookups(self):
        self.writeCmdToConsole("rootca4 = newCA(\"198.41.0.4\")")
        self.writeCmdToConsole("rootca6 = newCA(\"2001:503:ba3e::2:30\")")
        self.writeCmdToConsole("rblca = newCA(\"127.0.0.2\")")
        self.writeCmdToConsole("newDNSResolver(\"DNSTestResolv\")")
        self.writeCmdToConsole("resolv = getDNSResolver(\"DNSTestResolv\")")
        self.writeCmdToConsole("resolv:addResolver(\"208.67.222.222\", 53)")
        res = self.writeCmdToConsole("showAddrByName(resolv, \"a.root-servers.net.\")")
        if ((str.find(res, '198.41.0.4') == -1) and (str.find(res, "a.root-servers.net") != -1)):
            # this means there is a middlebox messing with us - replace 192.168.1.254 with your middlebox IP
            self.writeCmdToConsole("newDNSResolver(\"MiddleBoxResolv\")")
            self.writeCmdToConsole("resolv = getDNSResolver(\"MiddleBoxResolv\")")
            self.writeCmdToConsole("resolv:addResolver(\"192.168.1.254\", 53)")
            res = self.writeCmdToConsole("showAddrByName(resolv, \"a.root-servers.net.\")")
            
        self.assertNotEqual(str.find(res,'198.41.0.4'), -1)
        self.assertNotEqual(str.find(res,'2001:503:ba3e::2:30'), -1)
        res = self.writeCmdToConsole("showNameByAddr(resolv, rootca4)")
        self.assertNotEqual(str.find(res,'a.root-servers.net.'), -1)
        res = self.writeCmdToConsole("showNameByAddr(resolv, rootca6)")
        self.assertNotEqual(str.find(res,'a.root-servers.net.'), -1)
        res = self.writeCmdToConsole("showRBL(resolv, rblca, \"sbl.spamhaus.org.\")")
        self.assertNotEqual(str.find(res,'127.0.0.2'), -1)

    def test_DNSRetries(self):
        self.writeCmdToConsole("newDNSResolver(\"RetryResolv\")")
        self.writeCmdToConsole("resolv = getDNSResolver(\"RetryResolv\")")
        self.writeCmdToConsole("resolv:addResolver(\"208.67.222.222\", 5353)")
        self.writeCmdToConsole("resolv:addResolver(\"208.67.222.222\", 53)")
        # the DNS show... functions are hardcoded to retry once if encountering a timeout
        res = self.writeCmdToConsole("showAddrByName(resolv, \"a.root-servers.net.\")")
        if ((str.find(res, '198.41.0.4') == -1) and (str.find(res, "a.root-servers.net") != -1)):
            # this means there is a middlebox messing with us - replace 192.168.1.254 with your middlebox IP
            self.writeCmdToConsole("resolv = newDNSResolver(\"MiddleBoxRetryResolv\")")
            self.writeCmdToConsole("resolv = getDNSResolver(\"MiddleBoxRetryResolv\")")
            self.writeCmdToConsole("resolv:addResolver(\"192.168.1.254\", 5353)")
            self.writeCmdToConsole("resolv:addResolver(\"192.168.1.254\", 53)")
            res = self.writeCmdToConsole("showAddrByName(resolv, \"a.root-servers.net.\")")
        self.assertNotEqual(str.find(res,'198.41.0.4'), -1)
        
