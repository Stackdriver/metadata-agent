/*
 * Copyright 2017 Google Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 **/

//#include "config.h"

#include "api_server.h"
#include "configuration.h"
#include "docker.h"
#include "instance.h"
#include "kubernetes.h"

int main(int ac, char** av) {
  google::MetadataAgentConfiguration config;
  int parse_result = config.ParseArguments(ac, av);
  if (parse_result) {
    return parse_result < 0 ? 0 : parse_result;
  }

  google::MetadataAgent server(config);

  google::InstanceUpdater instance_updater(&server);
  google::DockerUpdater docker_updater(&server);
  google::KubernetesUpdater kubernetes_updater(&server);

  instance_updater.start();
  docker_updater.start();
  kubernetes_updater.start();

  server.start();
}
