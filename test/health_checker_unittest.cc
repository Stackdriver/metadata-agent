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

#include "../src/health_checker.h"
#include "../src/store.h"
#include "gtest/gtest.h"
#include <sstream>
#include <boost/filesystem.hpp>

namespace google {

class HealthCheckerUnittest : public ::testing::Test {
 protected:
  static void Cleanup(HealthChecker* health_checker){
    health_checker->CleanupForTest();
  }

  static void SetUnhealthy(HealthChecker* health_checker,
                           const std::string& state_name) {
    health_checker->SetUnhealthy(state_name);
  }

  static bool IsHealthy(const HealthChecker& health_checker) {
    return health_checker.IsHealthy();
  }

};

namespace {
std::istringstream IsolationPathConfig(const std::string& test_name) {
  return std::istringstream("HealthCheckFile: './" + test_name + "/unhealthy'");
}
}  // namespace

TEST_F(HealthCheckerUnittest, DefaultHealthy) {
  Configuration config(IsolationPathConfig(test_info_->name()));
  MetadataStore store(config);
  HealthChecker health_checker(config, store);
  EXPECT_TRUE(IsHealthy(health_checker));
  Cleanup(&health_checker);
}

TEST_F(HealthCheckerUnittest, SimpleFailure) {
  Configuration config(IsolationPathConfig(test_info_->name()));
  MetadataStore store(config);
  HealthChecker health_checker(config, store);
  SetUnhealthy(&health_checker, "kubernetes_pod_thread");
  EXPECT_FALSE(IsHealthy(health_checker));
  Cleanup(&health_checker);
}

TEST_F(HealthCheckerUnittest, MultiFailure) {
  Configuration config(IsolationPathConfig(test_info_->name()));
  MetadataStore store(config);
  HealthChecker health_checker(config, store);
  EXPECT_TRUE(IsHealthy(health_checker));
  SetUnhealthy(&health_checker, "kubernetes_pod_thread");
  EXPECT_FALSE(IsHealthy(health_checker));
  SetUnhealthy(&health_checker, "kubernetes_node_thread");
  EXPECT_FALSE(IsHealthy(health_checker));
  Cleanup(&health_checker);
}

TEST_F(HealthCheckerUnittest, FailurePersists) {
  Configuration config(IsolationPathConfig(test_info_->name()));
  MetadataStore store(config);
  HealthChecker health_checker(config, store);
  EXPECT_TRUE(IsHealthy(health_checker));
  SetUnhealthy(&health_checker, "kubernetes_pod_thread");
  EXPECT_FALSE(IsHealthy(health_checker));
  SetUnhealthy(&health_checker, "kubernetes_pod_thread");
  EXPECT_FALSE(IsHealthy(health_checker));
  Cleanup(&health_checker);
}
}  // namespace google
