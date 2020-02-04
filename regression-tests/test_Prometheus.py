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

    def getWforcePrometheusValues(self):
        r = self.getWforceMetrics()
        self.assertTrue(r)
        self.assertEquals(r.status_code, 200)

        return self.parsePrometheusResponse(r.text)

    def getTrackalertPrometheusValues(self):
        r = self.getTrackalertMetrics()
        self.assertTrue(r)
        self.assertEquals(r.status_code, 200)

        return self.parsePrometheusResponse(r.text)
    
    def checkWforceValues(self, values):
        for key in ['wforce_worker_queue_duration_seconds_bucket{le="0.001000"}', 'wforce_worker_response_duration_seconds_bucket{le="0.001000"}', 'wforce_commands_total{cmd="ping"}', 'wforce_custom_stats_total{metric="customStat"}', 'wforce_allow_status_total{status="denied"}', 'wforce_replication_sent_total{sibling="127.0.0.1:4001",status="ok"}', 'wforce_replication_tcp_connfailed_total{sibling="127.0.0.1:4001"}', 'wforce_redis_wlbl_updates_total{type="bl"}', 'wforce_redis_wlbl_connfailed_total{type="wl"}', 'wforce_bl_entries{type="iplogin"}', 'wforce_wl_entries{type="ip"}', 'wforce_web_queue_size', 'wforce_webhook_queue_size']:
            self.assertIn(key, values)
            self.assertGreaterEqual(float(values[key]), 0)

    def checkTrackalertValues(self, values):
        for key in ['trackalert_worker_queue_duration_seconds_bucket{le="0.001000"}', 'trackalert_worker_response_duration_seconds_bucket{le="0.001000"}', 'trackalert_active_http_connections', 'trackalert_commands_total{cmd="report"}']:
            self.assertIn(key, values)
            self.assertGreaterEqual(float(values[key]), 0)
            
    def test_WforceMetrics(self):
        values = self.getWforcePrometheusValues()
        self.checkWforceValues(values)

        allow_count = float(values['wforce_commands_total{cmd="allow"}'])
        
        # add one message before checking stats again
        self.allowFunc("foobar", "99.22.33.11", "1234")

        values = self.getWforcePrometheusValues()

        self.assertGreater(float(values['wforce_commands_total{cmd="allow"}']), allow_count)
        self.assertGreater(float(values['wforce_worker_response_duration_seconds_bucket{le="+Inf"}']), 0)

    def test_TrackalertMetrics(self):
        values = self.getTrackalertPrometheusValues()
        self.checkTrackalertValues(values)

        custom_count = float(values['trackalert_commands_total{cmd="custom"}'])
        
        # add one message before checking stats again
        self.trackalertCustomFunc("foobar")

        values = self.getTrackalertPrometheusValues()

        self.assertGreater(float(values['trackalert_commands_total{cmd="custom"}']), custom_count)
        self.assertGreater(float(values['trackalert_worker_response_duration_seconds_bucket{le="+Inf"}']), 0)

