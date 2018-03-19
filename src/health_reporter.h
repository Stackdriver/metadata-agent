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
#ifndef HEALTH_REPORTER_H_
#define HEALTH_REPORTER_H_

#include "logging.h"
#include <string>
#include <vector>

namespace google {

// Collects and reports health information about the metadata agent
class HealthReporter {
 public:
  HealthReporter();
  HealthReporter(const std::string &prefix);
  void SetUnhealthy(const std::string &state_name);
 private:
  friend class HealthReporterUnittest;

  bool ReportHealth();
  bool GetTotalHealthState();
  std::string Prefix(const std::string &state_name);

  std::vector<std::string> health_states;
  const std::string state_prefix;
};

} // google namespace

#endif  // HEALTH_REPORTER_H_
