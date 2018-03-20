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

#include "health_reporter.h"

#include <fstream>
#include <sstream>
#include <boost/algorithm/string/join.hpp>
#include <iostream>

namespace google {
namespace {
constexpr const char kExternalReportFilename[] = "metadata_agent_unhealthy";
}

HealthChecker::HealthChecker(const std::string& prefix, const MetadataAgentConfiguration& config)
  : state_prefix_(prefix), health_states_({"kubernetes_pod_thread", "kubernetes_node_thread"}), config_(config) {}

void HealthChecker::SetUnhealthy(const std::string &state_name) {
  TouchName(state_name);
  ReportHealth();
}

bool HealthChecker::ReportHealth() {
  if (!IsHealthy()) {
    TouchName(kExternalReportFilename);
    return false;
  }
  return true;
}

bool HealthChecker::IsHealthy() {
  for (const std::string& health_state : health_states_) {
    if (CheckName(health_state)) {
      return false;
    }
  }
  return true;
}

std::string HealthChecker::MakeHealthCheckPath(const std::string& file_name) {
  if (config_.HealthCheckLocation().length() > 0) {
    return boost::algorithm::join(std::vector<std::string>{config_.HealthCheckLocation(), file_name}, "/");
  }
  return file_name;
}

std::string HealthChecker::Prefix(const std::string& state_name) {
  return boost::algorithm::join(std::vector<std::string>{state_prefix_, state_name}, "_");
}

void HealthChecker::TouchName(const std::string& state_name) {
  std::string intermediate(Prefix(state_name));
  Touch(MakeHealthCheckPath(intermediate));
}

void HealthChecker::Touch(const std::string& path) {
    std::ofstream healthfile (path);
    healthfile << "\n";
}

bool HealthChecker::CheckName(const std::string& state_name) {
  return Check(MakeHealthCheckPath(Prefix(state_name)));
}

bool HealthChecker::Check(const std::string& path) {
    std::ifstream f(path);
    return f.good();
}

} // namespace google
