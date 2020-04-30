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

WFORCE_CMD="/usr/bin/wforce -D"

if [[ "$WFORCE_VERBOSE" -eq "1" ]]; then
    WFORCE_CMD="$WFORCE_CMD -v"
fi

if [[ "x$WFORCE_CONFIG_FILE" == "x" ]]; then
    if [[ "x$WFORCE_HTTP_PASSWORD" == "x" ]]; then
        2>&1 echo "WFORCE_HTTP_PASSWORD environment variable must be set"
    else
        echo "Note you are using the default config file - to take full advantage of the capabilities of weakforced, you should specify a custom config file with the WFORCE_CONFIG_FILE environment variable."
        create_config.sh /etc/wforce/wforce.conf.j2 /etc/wforce/wforce.conf
        echo "Starting $WFORCE_CMD"
        exec $WFORCE_CMD
    fi
else
    WFORCE_CMD="$WFORCE_CMD -C $WFORCE_CONFIG_FILE"
    exec $WFORCE_CMD
fi

exit 1



