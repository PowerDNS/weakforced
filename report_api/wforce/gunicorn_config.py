# Config file for the gunicorn HTTP server
# See: http://docs.gunicorn.org/en/stable/settings.html
#
#
import multiprocessing
import os
import sys

# WARNING: Use /etc/wforce/gunicorn_overrides.py to override these
bind = ['127.0.0.1:8448']
#bind = ['0.0.0.0:8448']
workers = min(multiprocessing.cpu_count() * 2, 32)
threads = 8 # per worker
timeout = 30 # sec

# Kill after this number of seconds on shutdown
graceful_timeout = 5

# Allow overriding gunicorn config settings in /etc
_overrides = '/etc/wforce/gunicorn_overrides.py'
if os.path.exists(_overrides):
    with open(_overrides) as f:
        _code = compile(f.read(), _overrides, 'exec')
        exec(_code, globals(), locals())
