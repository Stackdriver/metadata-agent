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

#include <map>
#include <mutex>
#include <set>
#include <string>

#include "configuration.h"

namespace google {

class MetadataStore;

// Collects and reports health information about the metadata agent.
class HealthChecker {
 public:
  HealthChecker(const Configuration& config, const MetadataStore& store);
  void SetUnhealthy(const std::string& component);
  std::set<std::string> UnhealthyComponents() const;

 private:
  friend class HealthCheckerUnittest;

  bool IsHealthy() const;
  void CleanupForTest();

  const Configuration& config_;
  const MetadataStore& store_;
  std::set<std::string> unhealthy_components_;
  mutable std::mutex mutex_;
};

}  // namespace google

#endif  // HEALTH_CHECKER_H_
