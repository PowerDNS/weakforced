#!/bin/bash

# Exit with error if one command fails
set -e

# Change to directory of script
cd "${0%/*}"
echo "Changed current directory to $PWD"

if [ ! -d "venv" ] || [ ! -e "venv/bin/activate" ]; then
    echo "* NOTE: ./venv/ does not exist yet, creating a new virtual env"
    python3 -m venv venv
fi

# Activate the virtual env before running any Django commands
source ./venv/bin/activate

pip install --upgrade pip
pip install -r requirements.txt
