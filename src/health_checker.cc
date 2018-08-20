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

#include "store.h"

#include <boost/filesystem.hpp>
#include <boost/algorithm/string/join.hpp>
#include <vector>
#include <fstream>

namespace google {

HealthChecker::HealthChecker(const Configuration& config,
                             const MetadataStore& store)
    : config_(config), store_(store) {
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
  return unhealthy_components_.empty();
}

void HealthChecker::CleanupForTest() {
  boost::filesystem::remove_all(boost::filesystem::path(
      config_.HealthCheckFile()).parent_path());
}

std::set<std::string> HealthChecker::UnhealthyComponents() const {
  std::lock_guard<std::mutex> lock(mutex_);
  std::set<std::string> result(unhealthy_components_);

  Timestamp cutoff = std::chrono::system_clock::now()
    - std::chrono::seconds(config_.HealthCheckMaxDataAgeSeconds());
  auto last_collection_times = store_.GetLastCollectionTimes();
  for (auto& kv : last_collection_times) {
    const std::string& object_type = kv.first;
    const Timestamp& collected_at = kv.second;
    if (collected_at < cutoff) {
      result.insert(object_type);
    }
  }
  return result;
}

}  // namespace google
