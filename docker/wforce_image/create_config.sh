#!/bin/bash

python3 -c 'import os
import sys
import jinja2
sys.stdout.write(
    jinja2.Template(sys.stdin.read()
).render(env=os.environ))' <$1 >$2
