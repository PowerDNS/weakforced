import subprocess
import time
from test_helper import ApiTestCase

ERROR = 3
WARNING = 4
INFO = 6
DEBUG = 7


class TestBasics(ApiTestCase):

    def check_loglevel(self, loglevel, content):
        cmd = ('../wforce/wforce -v -D -C ./wforce3.conf -R '
               '../wforce/regexes.yaml --loglevel %d' % loglevel).split()
        try:
            proc = subprocess.Popen(cmd, stdout=subprocess.PIPE)
            self.generate_log_entries()
        finally:
            proc.terminate()
            proc.wait()
        out, err = proc.communicate()
        if out:
            out = out.decode()
        if err:
            err = err.decode()
        assert content in out, out

    def trigger_log_entry(self, cmd):
        for i in range(20):
            out = self.writeCmdToConsole3(cmd).decode()
            if "Connection refused" not in out:
                break
            time.sleep(1)
        assert "Connection refused" not in out

    def generate_log_entries(self):
        self.trigger_log_entry('debugLog("Debug Test", {})')
        self.trigger_log_entry('infoLog("Info Test", {})')
        self.trigger_log_entry('warnLog("Warning Test", {})')
        self.trigger_log_entry('errorLog("Error Test", {})')

    def test_debug(self):
        self.check_loglevel(DEBUG, "Debug Test")

    def test_info(self):
        self.check_loglevel(INFO, "Info Test")

    def test_warning(self):
        self.check_loglevel(WARNING, "Warning Test")

    def test_error(self):
        self.check_loglevel(ERROR, "Error Test")
