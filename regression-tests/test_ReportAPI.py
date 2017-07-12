import requests
import socket
import re
import os
import time
import json
import mmap
from test_helper import ApiTestCase

class TestReportAPI(ApiTestCase):

    def setUp(self):
        self.writeCmdToConsole("addWebHook({'report'}, ls_ck)")

    def test_logins(self):
        r = self.reportFunc('report_api_test', '127.0.0.1', "1234", False)
        time.sleep(10)
        attrs = dict()
        attrs['login'] = "report_api_test@foobar.com"
        r = self.reportAPI("/logins", attrs)
        self.assertEquals(r.status_code, requests.codes.ok)
        print r.json()
        j=r.json()
        self.assertEquals(j['query']['login'], 'report_api_test@foobar.com')
        print j['response']
        id = j['response'][0]['id']
        attrs = dict()
        attrs['id'] = id
        attrs['confirm'] = True
        r = self.reportAPI("/logins/confirm", attrs)
        self.assertEquals(r.status_code, requests.codes.ok)

    def test_devices(self):
        r = self.reportFuncDeviceProtocol('report_api_test', '127.0.0.1', "1234", False, 'Mozilla/5.0 (Macintosh; Intel Mac OS X 10_9_3) AppleWebKit/537.75.14 (KHTML, like Gecko) Version/7.0.3 Safari/7046A194A', "http")
        time.sleep(10)
        attrs = dict()
        attrs['login'] = "report_api_test@foobar.com"
        r = self.reportAPI("/devices", attrs)
        self.assertEquals(r.status_code, requests.codes.ok)
        print r.json()
        j=r.json()
        self.assertEquals(j['query']['login'], 'report_api_test@foobar.com')

