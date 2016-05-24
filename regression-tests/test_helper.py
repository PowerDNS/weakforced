from datetime import datetime
import os
import requests
import urlparse
import unittest
import json
from subprocess import call, check_output

DAEMON = os.environ.get('DAEMON', 'authoritative')


class ApiTestCase(unittest.TestCase):

    def setUp(self):
        # TODO: config
        self.server_address = '127.0.0.1'
        self.server_port = int(os.environ.get('WEBPORT', '8084'))
        self.server_url = 'http://%s:%s/' % (self.server_address, self.server_port)
        self.session = requests.Session()
        self.session.auth = ('foo', os.environ.get('APIKEY', 'super'))
        #self.session.keep_alive = False
        #        self.session.headers = {'X-API-Key': os.environ.get('APIKEY', 'changeme-key'), 'Origin': 'http://%s:%s' % (self.server_address, self.server_port)}

    def writeFileToConsole(self, file):
        fp = open(file)
        cmds_nl = fp.read()
        # Lua doesn't need newlines and the console gets confused by them e.g.
        # function definitions
        cmds = cmds_nl.replace("\n", " ")
        return call(["../wforce", "-c", "../wforce.conf", "-e", cmds])

    def writeCmdToConsole(self, cmd):
        return check_output(["../wforce", "-c", "../wforce.conf", "-e", cmd])

    def allowFunc(self, login, remote, pwhash):
        return self.allowFuncAttrs(login, remote, pwhash, {})

    def allowFuncAttrs(self, login, remote, pwhash, attrs):
        payload = dict()
        payload['login'] = login
        payload['remote'] = remote
        payload['pwhash'] = pwhash
        payload['attrs'] = attrs
        return self.session.post(
            self.url("/?command=allow"),
            data=json.dumps(payload),
            headers={'Content-Type': 'application/json'}) 


    def reportFunc(self, login, remote, pwhash, success):
        return self.reportFuncAttrs(login, remote, pwhash, success, {})

    def reportFuncAttrs(self, login, remote, pwhash, success, attrs):
        payload = dict()
        payload['login'] = login
        payload['remote'] = remote
        payload['pwhash'] = pwhash
        payload['success'] = success
        payload['attrs'] = attrs
        return self.session.post(
            self.url("/?command=report"),
            data=json.dumps(payload),
            headers={'Content-Type': 'application/json'}) 

    def resetFunc(self, login, ip):
        payload = dict()
        payload['login'] = login
        payload['ip'] = ip
        return self.session.post(
            self.url("/?command=reset"),
            data=json.dumps(payload),
            headers={'Content-Type': 'application/json'}) 

    def pingFunc(self):
        return self.session.get(self.url("/?command=ping"))

    def getBLFunc(self):
        return self.session.get(self.url("/?command=getBL"))

    def getDBStatsIPLogin(self, ip, login):
        payload = dict()
        payload['login'] = login
        payload['ip'] = ip
        return self.session.post(
            self.url("/?command=getDBStats"),
            data=json.dumps(payload),
            headers={'Content-Type': 'application/json'}) 

    def getDBStatsIP(self, ip):
        payload = dict()
        payload['ip'] = ip
        return self.session.post(
            self.url("/?command=getDBStats"),
            data=json.dumps(payload),
            headers={'Content-Type': 'application/json'}) 

    def getDBStatsLogin(self, login):
        payload = dict()
        payload['login'] = login
        return self.session.post(
            self.url("/?command=getDBStats"),
            data=json.dumps(payload),
            headers={'Content-Type': 'application/json'}) 
    
    def url(self, relative_url):
        return urlparse.urljoin(self.server_url, relative_url)

    def assert_success_json(self, result):
        try:
            result.raise_for_status()
        except:
            print result.content
            raise
        self.assertEquals(result.headers['Content-Type'], 'application/json')
