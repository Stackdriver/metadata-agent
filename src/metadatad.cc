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

#include <functional>

#include "api_server.h"
#include "configuration.h"
#include "docker.h"
#include "kubernetes.h"
#include "updater.h"

int main(int ac, char** av) {
  google::MetadataAgentConfiguration config(ac > 1 ? av[1] : "");
  google::MetadataAgent server(config);

  google::DockerReader docker(config);
  google::PollingMetadataUpdater docker_updater(
      config.DockerUpdaterIntervalSeconds(), &server,
      std::bind(&google::DockerReader::MetadataQuery, &docker));

  google::KubernetesReader kubernetes(config);
  google::PollingMetadataUpdater kubernetes_updater(
      config.KubernetesUpdaterIntervalSeconds(), &server,
      std::bind(&google::KubernetesReader::MetadataQuery, &kubernetes));

  if (config.DockerUpdaterIntervalSeconds() > 0) {
    docker_updater.start();
  }
  if (config.KubernetesUpdaterIntervalSeconds() > 0) {
    kubernetes_updater.start();
  }
  server.start();
}
