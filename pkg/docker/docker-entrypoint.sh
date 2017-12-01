#!/bin/sh
set -e

# first arg is a flag.
if [ "${1:0:1}" = '-' ]; then
  set -- /opt/stackdriver/metadata/sbin/metadatad "$@"
fi

exec "$@"