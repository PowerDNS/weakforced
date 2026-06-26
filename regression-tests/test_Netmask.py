from test_helper import ApiTestCase


class TestNetmask(ApiTestCase):
    def test_new_netmask_from_ca_and_to_string_network(self):
        r = self.customFuncWithName("NetmaskHelpers", {})
        j = r.json()

        self.assertEqual(j["status"], "ok")
        self.assertEqual(j["r_attrs"]["v4_network"], "192.0.2.0/24")
        self.assertEqual(j["r_attrs"]["v6_network"], "2001:db8:abcd::/48")
        self.assertEqual(j["r_attrs"]["string_network"], "198.51.100.128/25")
