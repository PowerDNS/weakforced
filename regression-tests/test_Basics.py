import requests
import socket
import time
import json
from test_helper import ApiTestCase

configFile = "./wforce-tw.conf"

class TestBasics(ApiTestCase):

    def test_unauth(self):
        r = requests.get(self.url("/?command=stats"))
        self.assertEquals(r.status_code, requests.codes.unauthorized)

    def test_auth_stats(self):
        r = self.session.get(self.url("/?command=stats"))

    def test_ping(self):
        r = self.pingFunc()
        j = r.json()
        self.assertEquals(j['status'], 'ok')

    def test_getBL(self):
        r = self.getBLFunc()
        print r
        j = r.json()
        self.assertEquals(j['bl_entries'], [])

    def test_getDBStats(self):
        self.writeFileToConsole(configFile)
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
