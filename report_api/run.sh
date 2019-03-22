#!/usr/bin/env bash
set -e

if [ ! -d .venv ]; then
	python3 -m venv venv
fi
. venv/bin/activate
python -V
pip install --upgrade pip
pip install -r requirements.txt

set -e
set -x
gunicorn --bind 0.0.0.0:5000 runreport:app
