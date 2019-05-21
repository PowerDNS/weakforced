from datetime import datetime
import os
import requests
from urllib.parse import urlparse
from urllib.parse import urljoin
import unittest
import json
from subprocess import call, check_output

DAEMON = os.environ.get('DAEMON', 'authoritative')


class ApiTestCase(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        """On inherited classes, run our `setUp` method"""
        if cls is not ApiTestCase and cls.setUp is not ApiTestCase.setUp:
            orig_setUp = cls.setUp
            def setUpOverride(self, *args, **kwargs):
                ApiTestCase.setUp(self)
                return orig_setUp(self, *args, **kwargs)
            cls.setUp = setUpOverride

    def setUp(self):
        # TODO: config
        self.server_address = '127.0.0.1'
        self.server1_port = int(os.environ.get('WEBPORT', '8084'))
        self.server1_url = 'http://%s:%s/' % (self.server_address, self.server1_port)
        self.server2_port = 8085
        self.server2_url = 'http://%s:%s/' % (self.server_address, self.server2_port)
        self.server3_port = 8086
        self.server3_url = 'http://%s:%s/' % (self.server_address, self.server3_port)
        self.server4_port = 8087
        self.server4_url = 'http://%s:%s/' % (self.server_address, self.server4_port)
        self.ta_server_port = 8090
        self.ta_server_url = 'http://%s:%s/' % (self.server_address, self.ta_server_port)

        self.report_server_port = 5000
        self.report_server_url = 'http://%s:%s' % (self.server_address, self.report_server_port)
        
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
        return call(["../wforce/wforce", "-c", "./wforce1.conf", "-R", "../wforce/regexes.yaml", "-e", cmds])

    def writeCmdToConsole(self, cmd):
        return check_output(["../wforce/wforce", "-c", "./wforce1.conf", "-R", "../wforce/regexes.yaml", "-e", cmd])

    def writeCmdToConsole3(self, cmd):
        return check_output(["../wforce/wforce", "-c", "./wforce3.conf", "-R", "../wforce/regexes.yaml", "-e", cmd])
    
    def writeFileToConsoleReplica(self, file):
        fp = open(file)
        cmds_nl = fp.read()
        # Lua doesn't need newlines and the console gets confused by them e.g.
        # function definitions
        cmds = cmds_nl.replace("\n", " ")
        return call(["../wforce/wforce", "-c", "./wforce2.conf", "-R", "../wforce/regexes.yaml", "-e", cmds])

    def writeCmdToConsoleReplica(self, cmd):
        return check_output(["../wforce/wforce", "-c", "./wforce2.conf", "-R", "../wforce/regexes.yaml", "-e", cmd])
    
    def allowFunc(self, login, remote, pwhash):
        return self.allowFuncAttrsInternal(login, remote, pwhash, {}, "", "", False)

    def allowFuncAttrs(self, login, remote, pwhash, attrs):
        return self.allowFuncAttrsInternal(login, remote, pwhash, attrs, "", "", False)

    def allowFuncReplica(self, login, remote, pwhash):
        return self.allowFuncAttrsInternal(login, remote, pwhash, {}, "", "", True)
    
    def allowFuncAttrsReplica(self, login, remote, pwhash, attrs):
        return self.allowFuncAttrsInternal(login, remote, pwhash, attrs, "", "", True)

    def allowFuncReplica2(self, login, remote, pwhash):
        return self.allowFuncAttrsInternal(login, remote, pwhash, {}, "", "", True, True)
    
    def allowFuncAttrsReplica2(self, login, remote, pwhash, attrs):
        return self.allowFuncAttrsInternal(login, remote, pwhash, attrs, "", "", True, True)
    
    def allowFuncDeviceProtocol(self, login, remote, pwhash, device_id, protocol):
        return self.allowFuncAttrsInternal(login, remote, pwhash, {}, device_id, protocol, False)
    
    def allowFuncAttrsInternal(self, login, remote, pwhash, attrs, device_id, protocol, replica, replica2=False):
        payload = dict()
        payload['login'] = login
        payload['remote'] = remote
        payload['pwhash'] = pwhash
        payload['attrs'] = attrs
        payload['device_id'] = device_id
        payload['protocol'] = protocol
        payload['tls'] = False
        if not replica:
            return self.session.post(
                self.url("/?command=allow"),
                data=json.dumps(payload),
                headers={'Content-Type': 'application/json'})
        elif replica and not replica2:
            return self.session.post(
                self.url2("/?command=allow"),
                data=json.dumps(payload),
                headers={'Content-Type': 'application/json'})
        else:
            return self.session.post(
                self.url4("/?command=allow"),
                data=json.dumps(payload),
                headers={'Content-Type': 'application/json'})

        
    def reportFunc(self, login, remote, pwhash, success):
        return self.reportFuncAttrsInternal(login, remote, pwhash, success, {}, "", "", False)

    def reportFuncReplica(self, login, remote, pwhash, success):
        return self.reportFuncAttrsInternal(login, remote, pwhash, success, {}, "", "", True)
    
    def reportFuncAttrs(self, login, remote, pwhash, success, attrs):
        return self.reportFuncAttrsInternal(login, remote, pwhash, success, attrs, "", "", False)

    def reportFuncDeviceProtocol(self, login, remote, pwhash, success, device_id, protocol):
        return self.reportFuncAttrsInternal(login, remote, pwhash, success, {}, device_id, protocol, False)

    def reportFuncAttrsReplica(self, login, remote, pwhash, success, attrs):
        return self.reportFuncAttrsInternal(login, remote, pwhash, success, attrs, "", "", True)

    def reportFuncAttrsInternal(self, login, remote, pwhash, success, attrs, device_id, protocol, replica):
        payload = dict()
        payload['login'] = login
        payload['remote'] = remote
        payload['pwhash'] = pwhash
        payload['success'] = success
        payload['attrs'] = attrs
        payload['device_id'] = device_id
        payload['protocol'] = protocol
        if not replica:
            return self.session.post(
                self.url("/?command=report"),
                data=json.dumps(payload),
                headers={'Content-Type': 'application/json'})
        else:
            return self.session.post(
                self.url2("/?command=report"),
                data=json.dumps(payload),
                headers={'Content-Type': 'application/json'})            

    def taReportFuncAttrs(self, login, remote, pwhash, success, attrs):
        payload = dict()
        payload['login'] = login
        payload['remote'] = remote
        payload['pwhash'] = pwhash
        payload['success'] = success
        payload['attrs'] = attrs
        return self.session.post(
            self.ta_url("/?command=report"),
            data=json.dumps(payload),
            headers={'Content-Type': 'application/json'})
        
    def resetFunc(self, login, ip):
        return self.resetFuncInternal(login, ip, False)

    def resetFuncReplica(self, login, ip):
        return self.resetFuncInternal(login, ip, True)

    def resetFuncInternal(self, login, ip, replica):
        payload = dict()
        payload['login'] = login
        payload['ip'] = ip
        if not replica:
            return self.session.post(
                self.url("/?command=reset"),
                data=json.dumps(payload),
                headers={'Content-Type': 'application/json'})
        else:
            return self.session.post(
                self.url2("/?command=reset"),
                data=json.dumps(payload),
                headers={'Content-Type': 'application/json'})

    def incLogins(self, login):
        attrs = dict()
        attrs['login'] = login
        payload = dict()
        payload['attrs'] = attrs
        return self.session.post(
            self.url("/?command=incLogins"),
            data=json.dumps(payload),
            headers={'Content-Type': 'application/json'})

    def countLogins(self, login):
        return self.countLoginsInternal(login, False)

    def countLoginsReplica(self, login):
        return self.countLoginsInternal(login, True)

    def countLoginsReplica2(self, login):
        return self.countLoginsInternal(login, True, True)

    def countLoginsInternal(self, login, replica, replica2=False):
        attrs = dict()
        attrs['login'] = login
        payload = dict()
        payload['attrs'] = attrs
        if not replica:
            return self.session.post(
                self.url("/?command=countLogins"),
                data=json.dumps(payload),
                headers={'Content-Type': 'application/json'})
        elif replica and not replica2:
            return self.session.post(
                self.url2("/?command=countLogins"),
                data=json.dumps(payload),
                headers={'Content-Type': 'application/json'})
        else:
            return self.session.post(
                self.url4("/?command=countLogins"),
                data=json.dumps(payload),
                headers={'Content-Type': 'application/json'})

    def resetLogins(self, login):
        attrs = dict()
        attrs['login'] = login
        payload = dict()
        payload['attrs'] = attrs
        return self.session.post(
            self.url("/?command=resetLogins"),
            data=json.dumps(payload),
            headers={'Content-Type': 'application/json'})

    def customFunc(self, login):
        attrs = dict()
        attrs['login'] = login
        payload = dict()
        payload['attrs'] = attrs
        return self.session.post(
            self.url("/?command=custom"),
            data=json.dumps(payload),
            headers={'Content-Type': 'application/json'})

    def customGetFunc(self, func):
        return self.session.get(
            self.url("/?command=" + func))
    
    def customFuncWithName(self, custom_func_name, attrs):
        payload = dict()
        payload['attrs'] = attrs
        return self.session.post(
            self.url("/?command="+custom_func_name),
            data=json.dumps(payload),
            headers={'Content-Type': 'application/json'})

    def customFuncReplicaWithName(self, custom_func_name, attrs):
        payload = dict()
        payload['attrs'] = attrs
        return self.session.post(
            self.url2("/?command="+custom_func_name),
            data=json.dumps(payload),
            headers={'Content-Type': 'application/json'})
    
    def trackalertCustomFunc(self, login):
        attrs = dict()
        attrs['login'] = login
        payload = dict()
        payload['attrs'] = attrs
        return self.session.post(
            self.ta_url("/?command=custom"),
            data=json.dumps(payload),
            headers={'Content-Type': 'application/json'})
    
    def pingFunc(self):
        return self.session.get(self.url("/?command=ping"))

    def getBLFunc(self):
        return self.session.get(self.url("/?command=getBL"))

    def getWLFunc(self):
        return self.session.get(self.url("/?command=getWL"))

    def getBLFuncReplica(self):
        return self.session.get(self.url2("/?command=getBL"))

    def getWLFuncReplica(self):
        return self.session.get(self.url2("/?command=getWL"))
    
    def getBLFuncPersist(self):
        return self.session.get(self.url3("/?command=getBL"))

    def getWLFuncPersist(self):
        return self.session.get(self.url3("/?command=getWL"))
    
    def addBLEntryIPLogin(self, ip, login, expire_secs, reason):
        payload = dict()
        payload['login'] = login
        payload['ip'] = ip
        payload['expire_secs'] = expire_secs
        payload['reason'] = reason
        return self.session.post(
            self.url("/?command=addBLEntry"),
            data=json.dumps(payload),
            headers={'Content-Type': 'application/json'}) 

    def addBLEntryIP(self, ip, expire_secs, reason):
        payload = dict()
        payload['ip'] = ip
        payload['expire_secs'] = expire_secs
        payload['reason'] = reason
        return self.session.post(
            self.url("/?command=addBLEntry"),
            data=json.dumps(payload),
            headers={'Content-Type': 'application/json'}) 

    def addBLEntryNetmask(self, netmask, expire_secs, reason):
        payload = dict()
        payload['netmask'] = netmask
        payload['expire_secs'] = expire_secs
        payload['reason'] = reason
        return self.session.post(
            self.url("/?command=addBLEntry"),
            data=json.dumps(payload),
            headers={'Content-Type': 'application/json'}) 
    
    def addBLEntryIPPersist(self, ip, expire_secs, reason):
        payload = dict()
        payload['ip'] = ip
        payload['expire_secs'] = expire_secs
        payload['reason'] = reason
        return self.session.post(
            self.url3("/?command=addBLEntry"),
            data=json.dumps(payload),
            headers={'Content-Type': 'application/json'}) 
    
    def addBLEntryLogin(self, login, expire_secs, reason):
        payload = dict()
        payload['login'] = login
        payload['expire_secs'] = expire_secs
        payload['reason'] = reason
        return self.session.post(
            self.url("/?command=addBLEntry"),
            data=json.dumps(payload),
            headers={'Content-Type': 'application/json'}) 

    def delBLEntryIPLogin(self, ip, login):
        payload = dict()
        payload['login'] = login
        payload['ip'] = ip
        return self.session.post(
            self.url("/?command=delBLEntry"),
            data=json.dumps(payload),
            headers={'Content-Type': 'application/json'}) 

    def delBLEntryIP(self, ip):
        payload = dict()
        payload['ip'] = ip
        return self.session.post(
            self.url("/?command=delBLEntry"),
            data=json.dumps(payload),
            headers={'Content-Type': 'application/json'}) 

    def delBLEntryLogin(self, login):
        payload = dict()
        payload['login'] = login
        return self.session.post(
            self.url("/?command=delBLEntry"),
            data=json.dumps(payload),
            headers={'Content-Type': 'application/json'}) 

    def addWLEntryIPLogin(self, ip, login, expire_secs, reason):
        payload = dict()
        payload['login'] = login
        payload['ip'] = ip
        payload['expire_secs'] = expire_secs
        payload['reason'] = reason
        return self.session.post(
            self.url("/?command=addWLEntry"),
            data=json.dumps(payload),
            headers={'Content-Type': 'application/json'}) 

    def addWLEntryIP(self, ip, expire_secs, reason):
        payload = dict()
        payload['ip'] = ip
        payload['expire_secs'] = expire_secs
        payload['reason'] = reason
        return self.session.post(
            self.url("/?command=addWLEntry"),
            data=json.dumps(payload),
            headers={'Content-Type': 'application/json'}) 

    def addWLEntryNetmask(self, netmask, expire_secs, reason):
        payload = dict()
        payload['netmask'] = netmask
        payload['expire_secs'] = expire_secs
        payload['reason'] = reason
        return self.session.post(
            self.url("/?command=addWLEntry"),
            data=json.dumps(payload),
            headers={'Content-Type': 'application/json'}) 
    
    def addWLEntryIPPersist(self, ip, expire_secs, reason):
        payload = dict()
        payload['ip'] = ip
        payload['expire_secs'] = expire_secs
        payload['reason'] = reason
        return self.session.post(
            self.url3("/?command=addWLEntry"),
            data=json.dumps(payload),
            headers={'Content-Type': 'application/json'}) 
    
    def addWLEntryLogin(self, login, expire_secs, reason):
        payload = dict()
        payload['login'] = login
        payload['expire_secs'] = expire_secs
        payload['reason'] = reason
        return self.session.post(
            self.url("/?command=addWLEntry"),
            data=json.dumps(payload),
            headers={'Content-Type': 'application/json'}) 

    def delWLEntryIPLogin(self, ip, login):
        payload = dict()
        payload['login'] = login
        payload['ip'] = ip
        return self.session.post(
            self.url("/?command=delWLEntry"),
            data=json.dumps(payload),
            headers={'Content-Type': 'application/json'}) 

    def delWLEntryIP(self, ip):
        payload = dict()
        payload['ip'] = ip
        return self.session.post(
            self.url("/?command=delWLEntry"),
            data=json.dumps(payload),
            headers={'Content-Type': 'application/json'}) 

    def delWLEntryLogin(self, login):
        payload = dict()
        payload['login'] = login
        return self.session.post(
            self.url("/?command=delWLEntry"),
            data=json.dumps(payload),
            headers={'Content-Type': 'application/json'}) 
    
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

    def reportAPI(self, path, attrs):
        return self.session.post(
            self.report_url(path),
            data=json.dumps(attrs),
            headers={'Content-Type': 'application/json'}, auth=('foo', 'secret'))
    
    def url(self, relative_url):
        return urljoin(self.server1_url, relative_url)

    def url2(self, relative_url):
        return urljoin(self.server2_url, relative_url)

    def url3(self, relative_url):
        return urljoin(self.server3_url, relative_url)

    def url4(self, relative_url):
        return urljoin(self.server4_url, relative_url)
    
    def ta_url(self, relative_url):
        return urljoin(self.ta_server_url, relative_url)

    def report_url(self, relative_url):
        return urljoin(self.report_server_url, relative_url)
    
    def assert_success_json(self, result):
        try:
            result.raise_for_status()
        except:
            print(result.content)
            raise
        self.assertEquals(result.headers['Content-Type'], 'application/json')
