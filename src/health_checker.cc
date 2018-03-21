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

#include <boost/algorithm/string/join.hpp>
#include <fstream>
#include <iostream>
#include <sstream>

namespace google {

HealthChecker::HealthChecker(const MetadataAgentConfiguration& config)
    : health_states_({"kubernetes_pod_thread", "kubernetes_node_thread"}),
      config_(config) {
  InitialCleanup();
}

void HealthChecker::SetUnhealthy(const std::string& state_name) {
  TouchName(state_name);
  ReportHealth();
}

bool HealthChecker::ReportHealth() {
  if (!IsHealthy()) {
    TouchName(config_.HealthCheckExternalFileName());
    return false;
  }
  return true;
}

bool HealthChecker::IsHealthy() const {
  for (const std::string& health_state : health_states_) {
    if (CheckName(health_state)) {
      return false;
    }
  }
  return true;
}

std::string HealthChecker::MakeHealthCheckPath(const std::string& file_name) const {
  if (!config_.HealthCheckFileDirectory().empty()) {
    return boost::algorithm::join(
        std::vector<std::string>{config_.HealthCheckFileDirectory(), file_name}, "/");
  }
  return file_name;
}

void HealthChecker::TouchName(const std::string& state_name) {
  Touch(MakeHealthCheckPath(state_name));
}

void HealthChecker::Touch(const std::string& path) {
  std::ofstream health_file(path);
  health_file << std::endl;
}

bool HealthChecker::CheckName(const std::string& state_name) const {
  return Check(MakeHealthCheckPath(state_name));
}

bool HealthChecker::Check(const std::string& path) const {
  std::ifstream f(path);
  return f.good();
}

void HealthChecker::InitialCleanup() {
  std::remove(MakeHealthCheckPath(config_.HealthCheckExternalFileName()).c_str());
  for (const std::string& health_state : health_states_) {
    std::remove(MakeHealthCheckPath(health_state).c_str());
  }
}

}  // namespace google
