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

#include "api_server.h"
#include "configuration.h"
#include "health_checker.h"
#include "reporter.h"

namespace google {

MetadataAgent::MetadataAgent(const Configuration& config)
    : config_(config), store_(config_), health_checker_(config, store_) {}

MetadataAgent::~MetadataAgent() {}

void MetadataAgent::Start() {
  metadata_api_server_.reset(new MetadataApiServer(
      config_, &health_checker_, store_, config_.MetadataApiNumThreads(),
      config_.MetadataApiBindAddress(), config_.MetadataApiPort()));
  reporter_.reset(new MetadataReporter(
      config_, &store_, config_.MetadataReporterIntervalSeconds()));
}

void MetadataAgent::Stop() {
  metadata_api_server_->Stop();
  // TODO: Notify the metadata reporter as well.
}

}
