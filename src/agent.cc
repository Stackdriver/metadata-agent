/*
 * Copyright 2018 Google Inc.
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

#include "agent.h"

#include "configuration.h"
#include "api_server.h"
#include "reporter.h"
#include "health_checker.h"

namespace google {

MetadataAgent::MetadataAgent(const Configuration& config)
    : config_(config), store_(config_), health_checker_(config) {}

MetadataAgent::~MetadataAgent() {}

void MetadataAgent::start() {
  metadata_api_server_.reset(new MetadataApiServer(
      config_, store_, config_.MetadataApiNumThreads(), "0.0.0.0",
      config_.MetadataApiPort()));
  reporter_.reset(new MetadataReporter(
      config_, &store_, config_.MetadataReporterIntervalSeconds()));
}

void MetadataAgent::stop() {
  metadata_api_server_.reset();
  reporter_.reset();
}

}
