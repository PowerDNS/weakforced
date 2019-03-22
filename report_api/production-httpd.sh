#!/bin/sh

# Change to directory of script
cd "${0%/*}"

if [ -f "venv/bin/activate" ]; then
    . ./venv/bin/activate
fi

gunicorn -c wforce/gunicorn_config.py wforce:app
