import requests
import socket
import time
import json
import mmap
from test_helper import ApiTestCase

class TestBasics(ApiTestCase):

    def test_unauth(self):
        r = requests.get(self.url("/?command=stats"))
        self.assertEquals(r.status_code, requests.codes.unauthorized)

    def test_auth_stats(self):
        r = self.session.get(self.url("/?command=stats"))
        self.assertEquals(r.status_code, requests.codes.ok)

    def test_ping(self):
        r = self.pingFunc()
        j = r.json()
        self.assertEquals(j['status'], 'ok')

    def test_getBL(self):
        r = self.getBLFunc()
        j = r.json()
        self.assertEquals(j['bl_entries'], [])

    def test_customFunc(self):
        r = self.customFunc("custom1")
        j = r.json()
        self.assertEquals(j['r_attrs']['login'], 'custom1')

    def test_deviceParsing(self):
        r = self.allowFuncDeviceProtocol('foobar', '127.0.0.1', "12432", '"name" "Mac OS X Mail" "version" "10.0 (3226)" "os" "Mac OS X" "os-version" "10.12 (16A323)" "vendor" "Apple Inc."', "imap")
        j = r.json()
        self.assertRegexpMatches(json.dumps(j), "Mac OS X")
        r = self.allowFuncDeviceProtocol('foobar', '127.0.0.1', "12432", 'Mozilla/5.0 (Macintosh; Intel Mac OS X 10_9_3) AppleWebKit/537.75.14 (KHTML, like Gecko) Version/7.0.3 Safari/7046A194A', "http")
        j = r.json()
        self.assertRegexpMatches(json.dumps(j), "Mac OS X")
        r = self.allowFuncDeviceProtocol('foobar', '127.0.0.1', "12432", 'OpenXchange.Android.Mail/1.0+1234 (OS: 7.0; device: Samsung/GT9700)', "mobileapi")
        j = r.json()
        self.assertRegexpMatches(json.dumps(j), "Android")

    def test_getDBStats(self):
        self.reportFunc('dbstats', '1.4.3.2', '1234', False); 
        r = self.getDBStatsLogin('dbstats')
        j = r.json();
        self.assertRegexpMatches(json.dumps(j), "countLogins")
        r = self.getDBStatsIP('1.4.3.2')
        j = r.json();
        self.assertRegexpMatches(json.dumps(j), "countryCount")

    def chunkGen(self):
        payload = dict()
        payload['login'] = "chunky"
        payload['remote'] = "127.0.0.1"
        payload['pwhash'] = "1234"
        payload['success'] = True
        payload['attrs'] = {}
        yield json.dumps(payload)

    def testChunked(self):
        r = self.session.post(
            self.url("/?command=report"),
            data=self.chunkGen(),
            headers={'Content-Type': 'application/json'})
        j = r.json()
        self.assertEquals(j['status'], 'ok')
