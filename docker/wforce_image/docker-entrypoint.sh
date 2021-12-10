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

if [[ "x$TRACKALERT" == "x" ]]; then
    WFORCE_CMD="/usr/bin/wforce -D"

    if [[ "$WFORCE_VERBOSE" -eq "1" ]]; then
        WFORCE_CMD="$WFORCE_CMD -v"
    fi
    # Check if they just supplied their own wforce.conf file
    if [[ -f /etc/wforce/wforce.conf ]]; then
      echo "Using supplied wforce config file"
      WFORCE_CONFIG_FILE=/etc/wforce/wforce.conf
    fi
    if [[ "x$WFORCE_CONFIG_FILE" == "x" ]]; then
        if [[ "x$WFORCE_HTTP_PASSWORD" == "x" ]]; then
            2>&1 echo "WFORCE_HTTP_PASSWORD environment variable must be set"
        else
            echo "Note you are using the default config file - to take full advantage of the capabilities of weakforced, you should specify a custom config file with the WFORCE_CONFIG_FILE environment variable or just replace /etc/wforce/wforce.conf with your own config file."
            export WFORCE_KEY=`echo "makeKey()" | wforce | grep setKey`
            create_config.sh /etc/wforce/wforce.conf.j2 /etc/wforce/wforce.conf
            echo "Starting $WFORCE_CMD"
            exec $WFORCE_CMD
        fi
    else
        WFORCE_CMD="$WFORCE_CMD -C $WFORCE_CONFIG_FILE"
        exec $WFORCE_CMD
    fi
else
    TRACKALERT_CMD="/usr/bin/trackalert -D"

    if [[ "$WFORCE_VERBOSE" -eq "1" ]]; then
        TRACKALERT_CMD="$TRACKALERT_CMD -v"
    fi

    if [[ "x$TRACKALERT_CONFIG_FILE" == "x" ]]; then
        if [[ "x$TRACKALERT_HTTP_PASSWORD" == "x" ]]; then
            2>&1 echo "TRACKALERT_HTTP_PASSWORD environment variable must be set"
        else
            echo "Note you are using the default config file - to take full advantage of the capabilities of trackalert, you should specify a custom config file with the TRACKALERT_CONFIG_FILE environment variable or just replace /etc/wforce/trackalert.conf with your own config file."
            export TRACKALERT_KEY=`echo "makeKey()" | trackalert | grep setKey`
            create_config.sh /etc/wforce/trackalert.conf.j2 /etc/wforce/trackalert.conf
            echo "Starting $TRACKALERT_CMD"
            exec $TRACKALERT_CMD
        fi
    else
        TRACKALERT_CMD="$WFORCE_CMD -C $TRACKALERT_CONFIG_FILE"
        exec $TRACKALERT_CMD
    fi
fi

exit 1



