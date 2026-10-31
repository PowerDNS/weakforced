import json
import os
import shutil
import subprocess
import tempfile
import time
import pytest
from mimesis import Internet
from urllib.parse import urljoin
from test_helper import ApiTestCase
from test_helper import running_process_on_port
from test_helper import start_process
from test_helper import terminate_process
from test_helper import wait_for_port

MASTER_PORT = 6391
REPLICA_PORT = 6392
SENTINEL_PORT = 26391
MASTER_NAME = "wfmaster"
REDIS_PASSWORD = "redispw"
SENTINEL_PASSWORD = "sentinelpw"


def redis_cli(port, *args, password=REDIS_PASSWORD):
    cmd = ["redis-cli", "-p", str(port), "--no-auth-warning"]
    if password:
        cmd += ["-a", password]
    return subprocess.check_output(cmd + list(args), text=True).strip()


def sentinel_master_port():
    out = redis_cli(SENTINEL_PORT, "SENTINEL", "get-master-addr-by-name", MASTER_NAME,
                    password=SENTINEL_PASSWORD)
    return int(out.split()[-1])


def count_keys(port, prefix):
    return len([k for k in redis_cli(port, "--scan", "--pattern", prefix + ":*").split("\n") if k])


@pytest.mark.skipif(shutil.which('redis-server') is None or shutil.which('redis-sentinel') is None,
                    reason="redis-server/redis-sentinel is not installed")
class TestBlacklistSentinel(ApiTestCase):
    # Runs a local redis master, a replica and a sentinel, and checks that
    # wforce finds the master via the sentinel, and keeps persisting entries
    # to the new master after a failover.
    procs = []
    tmpdir = None

    @classmethod
    def startRedis(cls, args, port):
        proc = start_process(args, close_fds=True)
        cls.procs.append(proc)
        if not wait_for_port(port):
            raise RuntimeError("redis did not start on port %d" % port)

    @classmethod
    def setUpClass(cls):
        cls.tmpdir = tempfile.mkdtemp()
        auth = ["--requirepass", REDIS_PASSWORD, "--masterauth", REDIS_PASSWORD]
        try:
            cls.startRedis(["redis-server", "--port", str(MASTER_PORT), "--save", "",
                            "--dir", cls.tmpdir] + auth, MASTER_PORT)
            cls.startRedis(["redis-server", "--port", str(REPLICA_PORT), "--save", "",
                            "--dir", cls.tmpdir, "--replicaof", "127.0.0.1", str(MASTER_PORT)] + auth,
                           REPLICA_PORT)
            # sentinel rewrites its config file, so it needs its own writable copy
            sentinel_conf = os.path.join(cls.tmpdir, "sentinel.conf")
            with open(sentinel_conf, "w") as f:
                f.write("port %d\n" % SENTINEL_PORT)
                f.write("requirepass %s\n" % SENTINEL_PASSWORD)
                f.write("sentinel monitor %s 127.0.0.1 %d 1\n" % (MASTER_NAME, MASTER_PORT))
                f.write("sentinel auth-pass %s %s\n" % (MASTER_NAME, REDIS_PASSWORD))
                f.write("sentinel down-after-milliseconds %s 1000\n" % MASTER_NAME)
                f.write("sentinel failover-timeout %s 5000\n" % MASTER_NAME)
            cls.startRedis(["redis-sentinel", sentinel_conf], SENTINEL_PORT)
            # Wait for the replica to be in sync, so that a failover can promote it
            for _ in range(40):
                if "master_link_status:up" in redis_cli(REPLICA_PORT, "INFO", "replication"):
                    break
                time.sleep(0.5)
            else:
                raise RuntimeError("redis replica did not sync with the master")
        except Exception:
            cls.tearDownClass()
            raise

    @classmethod
    def tearDownClass(cls):
        for proc in cls.procs:
            terminate_process(proc)
        cls.procs = []
        if cls.tmpdir is not None:
            shutil.rmtree(cls.tmpdir, ignore_errors=True)

    def url7(self, relative_url):
        return urljoin('http://%s:%s/' % (self.server_address, 8091), relative_url)

    def addBLEntryIP(self, ip):
        payload = {'ip': ip, 'expire_secs': 60, 'reason': "test sentinel blacklist"}
        return self.session.post(self.url7("/?command=addBLEntry"), data=json.dumps(payload),
                                 headers={'Content-Type': 'application/json'})

    def addEntries(self, num):
        for _ in range(num):
            r = self.addBLEntryIP(Internet().ip_v4())
            self.assertEqual(r.json()['status'], 'ok')

    def test_PersistBlacklistSentinel(self):
        cmd7 = ("../wforce/wforce -D -C ./wforce7.conf -R ../wforce/regexes.yaml").split()
        old_master = sentinel_master_port()
        new_master = REPLICA_PORT if old_master == MASTER_PORT else MASTER_PORT
        before = count_keys(old_master, "wfbl")

        with running_process_on_port(cmd7, 8091, close_fds=True):
            self.addEntries(50)
            self.assertEqual(count_keys(old_master, "wfbl"), before + 50)

            # Fail over while wforce is connected to the old master. Wait until the old master
            # has been turned into a replica, as writes to it before that are lost (as with any redis client)
            redis_cli(SENTINEL_PORT, "SENTINEL", "FAILOVER", MASTER_NAME, password=SENTINEL_PASSWORD)
            for _ in range(40):
                if sentinel_master_port() == new_master and \
                   "role:master" in redis_cli(new_master, "INFO", "replication") and \
                   "role:slave" in redis_cli(old_master, "INFO", "replication"):
                    break
                time.sleep(0.5)
            else:
                self.fail("sentinel failover did not complete")

            # These must all reach the new master, even though the existing connection was killed
            self.addEntries(50)
            self.assertEqual(count_keys(new_master, "wfbl"), before + 100)

        # The entries should be read back from the new master on startup
        with running_process_on_port(cmd7, 8091, close_fds=True):
            time.sleep(1)
            r = self.session.get(self.url7("/?command=getBL"))
            self.assertEqual(len(r.json()['bl_entries']), before + 100)
