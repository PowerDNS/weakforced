import requests
import socket
import time
import json
from test_helper import ApiTestCase


class TestBlock(ApiTestCase):

    def test_block(self):
        payload = dict()
        payload['login'] = 'baddie'
        payload['remote'] = '127.0.0.1'
        payload['pwhash'] = "1234"
        r = self.session.post(
            self.url("/?command=allow"),
            data=json.dumps(payload),
            headers={'content-type': 'application/x-www-form-urlencoded'}) # FIXME: content-type should be something/json but that kills wforce
#        self.assert_success_json(r)
        j = r.json()
        self.assertEquals(j['status'], 0)
        r.close()

        for i in range(100):
            print i
            payload = dict()
            payload['login'] = 'baddie'
            payload['remote'] = '127.0.0.1'
            payload['pwhash'] = "1234'%s" % i
            payload['success'] = 'false'
            r = self.session.post(
                self.url("/?command=report"),
                data=json.dumps(payload),
                headers={'content-type': 'application/x-www-form-urlencoded'}) #FIXME see above
            r.json()


        payload = dict()
        payload['login'] = 'baddie'
        payload['remote'] = '127.0.0.1'
        payload['pwhash'] = "1234"
        r = self.session.post(
            self.url("/?command=allow"),
            data=json.dumps(payload),
            headers={'content-type': 'application/x-www-form-urlencoded'}) #FIXME see above
#        self.assert_success_json(r)
        j = r.json()
        self.assertEquals(j['status'], -1)
