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
#ifndef HEALTH_CHECKER_H_
#define HEALTH_CHECKER_H_

#include <string>
#include <vector>

#include "configuration.h"

namespace google {

// Collects and reports health information about the metadata agent.
class HealthChecker {
 public:
  HealthChecker(const MetadataAgentConfiguration& config);
  void SetUnhealthy(const std::string& state_name);

 private:
  friend class HealthCheckerUnittest;

  bool ReportHealth();
  bool IsHealthy() const;
  std::string MakeHealthCheckPath(const std::string& file_name) const;
  void TouchName(const std::string& state_name);
  bool CheckName(const std::string& state_name) const;
  void Touch(const std::string& path);
  bool Check(const std::string& path) const;
  void InitialCleanup();

  std::vector<std::string> health_states_;
  const MetadataAgentConfiguration& config_;
};

}  // namespace google

#endif  // HEALTH_CHECKER_H_
