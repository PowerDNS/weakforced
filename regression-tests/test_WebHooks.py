import requests
import mmap
import re
import time
import os
import json
from test_helper import ApiTestCase

class TestWebHooks(ApiTestCase):

    def test_webhooks(self):
        self.writeCmdToConsole("addWebHook(events, ck)")
        self.reportFunc('webhooktest', '1.4.3.1', '1234', False)
        self.allowFunc('webhooktest', '1.4.3.2', '1234')
        self.resetFunc('webhooktest', '1.4.3.3'); 
        self.addBLEntryLogin('webhooktest', 10, 'This is not a reason')
        time.sleep(11)
        self.addBLEntryLogin('webhooktest', 10, 'This is not a reason')
        self.delBLEntryLogin('webhooktest')
        time.sleep(5)
        logfile = open('/tmp/webhook-server.log', 'r')
        s = mmap.mmap(logfile.fileno(), 0, access=mmap.ACCESS_READ)
        search_str = s.read(s.size()).decode()
        for event in [ 'report', 'allow', 'reset', 'addbl', 'delbl', 'expirebl' ]:
            regex = r"digest_match=True, event=" + re.escape(event)
            result = re.search(regex, search_str);
            self.assertNotEqual(result, None)
        s.close()
        logfile.close()

    def test_mtls_webhook(self):
        self.writeCmdToConsole("addWebHook(events, mtls_ck)")
        self.reportFunc('mtls_webhook_test', '1.4.3.1', '1234', False)
        self.allowFunc('mtls_webhook_test2', '1.4.3.2', '1234')
        self.resetFunc('mtls_webhook_test2', '1.4.3.3');
        self.addBLEntryLogin('mtls_webhook_test2', 10, 'This is not a reason')
        self.addBLEntryLogin('mtls_webhook_test2', 10, 'This is not a reason')
        self.delBLEntryLogin('mtls_webhook_test2')
        time.sleep(4)
        logfile = open('/var/log/nginx/access.log', 'r')
        s = mmap.mmap(logfile.fileno(), 0, access=mmap.ACCESS_READ)
        search_str = s.read(s.size()).decode()
        self.assertIn('"POST / HTTP/1.1" 200', search_str)
        s.close()
        logfile.close()

    def test_kafka_webhooks(self):
        self.writeCmdToConsole("addWebHook(events, kafka_ck)")
        self.reportFunc('kafkawebhooktest', '4.4.3.1', '1234', False)
        self.reportFunc('kafkawebhooktest', '4.4.3.1', '1234', False)
        time.sleep(5)
        r = self.kafkaProducer()
        j = r.json()
        print(json.dumps(j))
        self.assertGreater(j['offsets'][0]['offset'], 0)
        
    def test_customwebhooks(self):
        self.writeCmdToConsole("addCustomWebHook(\"customwebhook\", ck)")
        r = self.customFunc("custom1")
        j = r.json()
        self.assertEqual(j['r_attrs']['login'], 'custom1')
        time.sleep(5)
        logfile = open('/tmp/webhook-server.log', 'r')
        s = mmap.mmap(logfile.fileno(), 0, access=mmap.ACCESS_READ)
        search_str = s.read(s.size()).decode()
        for event in [ 'customwebhook' ]:
            regex = r"digest_match=True, event=" + re.escape(event)
            result = re.search(regex, search_str);
            self.assertNotEqual(result, None)
        s.close()
        logfile.close()

    def test_namedreportsinks(self):
        self.writeCmdToConsole("addNamedReportSink(\"trackalert\", \"127.0.0.1:4502\")")
        r = self.reportFunc('namedreportsink', '1.2.3.4', '1234', False)
        r = self.customFunc("customargs")
        time.sleep(1)
        logfile = open('/tmp/udp-sink.log', 'r')
        s = mmap.mmap(logfile.fileno(), 0, access=mmap.ACCESS_READ)
        search_str = s.read(s.size()).decode()
        for event in [ 'namedreportsink', 'customargs' ]:
            regex = r"\"login\": \"" + re.escape(event)
            result = re.search(regex, search_str);
            self.assertNotEqual(result, None)
        s.close()
        logfile.close()
