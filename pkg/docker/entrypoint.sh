#!/bin/bash

# Copyright 2018 Google Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

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

# Note: substring substitution is a bash-ism.
if [ "${1:0:1}" = '-' ]; then
  set -- /opt/stackdriver/metadata/sbin/metadatad "$@"
fi

exec "$@"
