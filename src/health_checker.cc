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

#include "health_checker.h"

#include <boost/filesystem.hpp>
#include <boost/algorithm/string/join.hpp>
#include <vector>
#include <fstream>

namespace {
  constexpr const char* kHealthStates[] = {
    "kubernetes_pod_thread", "kubernetes_node_thread"
  };
}
namespace google {

HealthChecker::HealthChecker(const MetadataAgentConfiguration& config)
    : config_(config) {
  boost::filesystem::create_directories(config_.HealthCheckFileDirectory());
  Remove(config_.HealthCheckFileDirectory(),
         config_.HealthCheckExternalFileName());
}

void HealthChecker::SetUnhealthyStateName(const std::string& state_name) {
  std::lock_guard<std::mutex> lock(mutex_);
  health_states_.insert(state_name);
  Touch(config_.HealthCheckFileDirectory(),
        config_.HealthCheckExternalFileName());
}

bool HealthChecker::IsHealthy() {
  std::lock_guard<std::mutex> lock(mutex_);
  return health_states_.size() == 0;
}

void HealthChecker::Touch(const std::string& directory, const std::string& name) {
  std::string path(boost::algorithm::join(
      std::vector<std::string>{directory, name}, "/"));
  std::ofstream health_file(path);
  health_file << std::endl;
}

void HealthChecker::Remove(const std::string& directory, const std::string& name) {
  std::string path(boost::algorithm::join(
      std::vector<std::string>{directory, name}, "/"));
  std::remove(path.c_str());
}

}  // namespace google
