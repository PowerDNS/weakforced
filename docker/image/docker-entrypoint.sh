#!/bin/bash
set -e

umask 0002

# Allow user to specify custom CMD
if [[ "$1" == "wfwrapper" ]]; then
  # Rewrite CMD args to remove the explicit command,
  set -- "${@:2}"
else
  # Run whatever command the user wanted
  exec "$@"
fi

exec /usr/bin/wforce -s


