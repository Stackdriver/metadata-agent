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

HealthChecker::HealthChecker(const Configuration& config)
    : config_(config) {
  boost::filesystem::create_directories(
      boost::filesystem::path(config_.HealthCheckFile()).parent_path());
  std::remove(config_.HealthCheckFile().c_str());
}

void HealthChecker::SetUnhealthy(const std::string& component) {
  std::lock_guard<std::mutex> lock(mutex_);
  unhealthy_components_.insert(component);
  std::ofstream health_file(config_.HealthCheckFile());
  health_file << std::endl;
}

bool HealthChecker::IsHealthy() const {
  std::lock_guard<std::mutex> lock(mutex_);
  if (!unhealthy_components_.empty()) {
    return false;
  }
  for (auto& c : component_callbacks_) {
    if (!c.second()) {
      return false;
    }
  }
  return true;
}

void HealthChecker::CleanupForTest() {
  boost::filesystem::remove_all(boost::filesystem::path(
      config_.HealthCheckFile()).parent_path());
}

void HealthChecker::RegisterCallback(const std::string& component,
                                     std::function<bool()> callback) {
  std::lock_guard<std::mutex> lock(mutex_);
  component_callbacks_[component] = callback;
}

void HealthChecker::UnregisterCallback(const std::string& component) {
  std::lock_guard<std::mutex> lock(mutex_);
  component_callbacks_.erase(component);
}

}  // namespace google
