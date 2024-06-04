#!/usr/bin/env bash

SCRIPTROOT="$( cd "$(dirname "$0")" ; pwd -P )"
cd "$SCRIPTROOT/.."

set -ex

dockerfile=$(mktemp -t Dockerfile-test.XXXXXX)

# Older versions do not always exit with code 0 on success, so we 
# cannot set -o pipefail.
./builder/templating/templating.sh luaenv/Dockerfile-test.template > "$dockerfile"

docker build -f "$dockerfile" "$@" .

# Will not get removed if an error occurs, useful for debugging
rm "$dockerfile"
