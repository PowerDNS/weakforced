#!/usr/bin/env bash
set -e

if [ ! -d .venv ]; then
	virtualenv .venv
fi
. .venv/bin/activate
python -V
pip install -r requirements.txt

set -e
set -x
exec ./runreport.py "$@"
