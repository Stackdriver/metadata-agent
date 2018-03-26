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

namespace google {

HealthChecker::HealthChecker(const MetadataAgentConfiguration& config)
    : config_(config) {
  boost::filesystem::create_directories(
    boost::filesystem::path(config_.HealthCheckFile()).parent_path());
  Remove(config_.HealthCheckFile());
}

void HealthChecker::SetUnhealthy(const std::string& state_name) {
  std::lock_guard<std::mutex> lock(mutex_);
  health_states_.insert(state_name);
  Touch(config_.HealthCheckFile());
}

bool HealthChecker::IsHealthy() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return health_states_.empty();
}

void HealthChecker::Touch(const std::string& path) {
  std::ofstream health_file(path);
  health_file << std::endl;
}

void HealthChecker::Remove(const std::string& path) {
  std::remove(path.c_str());
}

}  // namespace google
