import requests
import time
import os
import requests
from urllib.parse import urlparse
from urllib.parse import urljoin

from prometheus_client.parser import text_string_to_metric_families
from prometheus_client.samples import Sample
from test_helper import ApiTestCase

class TestPrometheus(ApiTestCase):

    def parsePrometheusResponse(self, response):
        # parse the text feed and iterate over samples so we make sure
        # the content is well formated
        for family in text_string_to_metric_families(response):
            for sample in family.samples:
                self.assertTrue(isinstance(sample, Sample))
        result = {}
        for line in response.split("\n"):
            if not line or line.startswith('#'):
                continue
            values = line.split(" ")
            self.assertGreaterEqual(len(values), 2)
            result[" ".join(values[0:-1])] = values[-1]

        return result

    def getReplfwdPrometheusValues(self):
        r = self.getReplfwdMetrics()
        self.assertTrue(r)
        self.assertEqual(r.status_code, 200)

        return self.parsePrometheusResponse(r.text)

    def checkReplfwdValues(self, values):
        for key in ['replfwd_worker_queue_duration_seconds_bucket{le="0.001"}', 'replfwd_worker_response_duration_seconds_bucket{le="0.001"}', 'replfwd_replication_sent_total{sibling="127.0.0.1:4001",status="ok"}', 'replfwd_replication_rcvd_total{sibling="127.0.0.1",status="ok"}', 'replfwd_replication_tcp_connfailed_total{sibling="127.0.0.1:4001"}']:
            self.assertIn(key, values)
            self.assertGreaterEqual(float(values[key]), 0)

    def test_ReplfwdMetrics(self):
        values = self.getReplfwdPrometheusValues()
        self.checkReplfwdValues(values)

        repl_sent_count = float(values['replfwd_replication_sent_total{sibling="127.0.0.1:4001",status="ok"}'])
        repl_rcvd_count = float(values['replfwd_replication_rcvd_total{sibling="127.0.0.1",status="ok"}'])
        # add one message in both directions before checking stats again
        self.reportFunc("foobar", "99.22.33.11", "1234", False)
        self.reportFuncReplica("barfoo", "99.22.33.1", "1234", False)

        time.sleep(1)
        
        values = self.getReplfwdPrometheusValues()

        self.assertGreater(float(values['replfwd_replication_sent_total{sibling="127.0.0.1:4001",status="ok"}']), repl_sent_count)
        self.assertGreater(float(values['replfwd_replication_rcvd_total{sibling="127.0.0.1",status="ok"}']), repl_rcvd_count)

