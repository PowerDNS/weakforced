#!/bin/bash

# Dir of script
ROOT="$( cd "$(dirname "$0")" ; pwd -P )"

# Safety check
if [ -z "$ROOT" ] || [ ! -x "$ROOT/clean.sh" ] ; then
    echo "ERROR: unexpected ROOT"
    exit 9
fi

if [ ! -d "target" ] && [ -d "src" ]; then
    echo "WARNING: no target/ dir found here. This only cleans devenvs, not deployment roots."
fi

set -x
rm -rf "$ROOT/src" "$ROOT/target"
rm -f "$ROOT/bin"
