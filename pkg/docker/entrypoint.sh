#!/bin/bash
set -e

# This docker image supports sending either a flag or a command as the docker
# command. When a flag is sent, it will be passed on to the metadata agent
# process. Anything else will be interpreted as the command to be run.
#
# Passing a flag.
# $ docker run -it {image:tag} -v
#
# Passing a command.
# $ docker run -it {image:tag} /bin/bash
#
# Default behavior uses CMD defined in Dockerfile.
# $ docker run -it {image:tag}

if [ "${1:0:1}" = '-' ]; then
  set -- /opt/stackdriver/metadata/sbin/metadatad "$@"
fi

exec "$@"
