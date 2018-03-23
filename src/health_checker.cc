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
#include <fstream>

namespace google {

HealthChecker::HealthChecker(const MetadataAgentConfiguration& config)
    : config_(config) {
  boost::filesystem::create_directories(config_.HealthCheckFileDirectory());
  FileWrapper::Remove(config_.HealthCheckFileDirectory(),
                      config_.HealthCheckExternalFileName());
}

void HealthChecker::SetUnhealthyStateName(const std::string& state_name) {
  if (health_files_.count(state_name) == 0) {
    health_files_[state_name].reset(
        new FileWrapper(config_.HealthCheckFileDirectory(), state_name));
  }
  ReportHealth();
}

bool HealthChecker::ReportHealth() {
  if (!IsHealthy()) {
    FileWrapper::Touch(config_.HealthCheckFileDirectory(),
                       config_.HealthCheckExternalFileName());
    return false;
  }
  return true;
}

bool HealthChecker::IsHealthy() const {
  for (int i = 0; i < sizeof(kHealthStates)/sizeof(char*); ++i) {
    if (CheckStateName(kHealthStates[i])) {
      return false;
    }
  }
  return true;
}

bool HealthChecker::CheckStateName(const std::string& state_name) const {
  return FileWrapper::Exists(config_.HealthCheckFileDirectory(), state_name);
}

}  // namespace google
