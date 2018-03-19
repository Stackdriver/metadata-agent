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

namespace google {
  const std::string external_report_filename = "metadata_agent_unhealthy";

  HealthReporter::HealthReporter() : HealthReporter::HealthReporter("") { }

  HealthReporter::HealthReporter(const std::string &prefix) : state_prefix(prefix) {
    health_states.push_back("watch_pods");
    health_states.push_back("node_callback");
  }

  void HealthReporter::SetUnhealthy(const std::string &state_name) {
    std::ofstream healthfile (Prefix(state_name));
    healthfile << "\n";
    healthfile.close();

    ReportHealth();
  }

  bool HealthReporter::ReportHealth() {
    if(!GetTotalHealthState()) {
      std::ofstream healthfile (Prefix(external_report_filename));
      healthfile << "\n";
      healthfile.close();
      return false;
    }
    return true;
  }

  bool HealthReporter::GetTotalHealthState() {
    for (std::string healthState : health_states) {
      std::ifstream f(Prefix(healthState));
      if(f.good()) {
        return false;
      }
    }
    return true;
  }

  std::string HealthReporter::Prefix(const std::string &state_name) {
    std::ostringstream oss;
    oss << state_prefix << "_" << state_name;
    return oss.str();
  }
} // namespace google
