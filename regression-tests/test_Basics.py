import requests
import socket
import time
from test_helper import ApiTestCase


class TestBasics(ApiTestCase):

    def test_unauth(self):
        r = requests.get(self.url("/?command=stats"))
        self.assertEquals(r.status_code, requests.codes.unauthorized)

    def test_auth_stats(self):
        r = self.session.get(self.url("/?command=stats"))
