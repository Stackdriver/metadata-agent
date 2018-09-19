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
#include "../src/time.h"
#include "gtest/gtest.h"
#include <sstream>

namespace google {

class HealthCheckerUnittest : public ::testing::Test {
 protected:
  static void SetUnhealthy(HealthChecker* health_checker,
                           const std::string& state_name) {
    health_checker->SetUnhealthy(state_name);
  }
};

TEST_F(HealthCheckerUnittest, DefaultHealthy) {
  Configuration config;
  MetadataStore store(config);
  HealthChecker health_checker(config, store);
  EXPECT_TRUE(health_checker.UnhealthyComponents().empty());
}

TEST_F(HealthCheckerUnittest, SimpleFailure) {
  Configuration config;
  MetadataStore store(config);
  HealthChecker health_checker(config, store);
  SetUnhealthy(&health_checker, "kubernetes_pod_thread");
  EXPECT_EQ(health_checker.UnhealthyComponents(),
            std::set<std::string>({
                "kubernetes_pod_thread",
            }));
}

TEST_F(HealthCheckerUnittest, MultiFailure) {
  Configuration config;
  MetadataStore store(config);
  HealthChecker health_checker(config, store);
  EXPECT_TRUE(health_checker.UnhealthyComponents().empty());
  SetUnhealthy(&health_checker, "kubernetes_pod_thread");
  EXPECT_EQ(health_checker.UnhealthyComponents(),
            std::set<std::string>({
                "kubernetes_pod_thread",
            }));
  SetUnhealthy(&health_checker, "kubernetes_node_thread");
  EXPECT_EQ(health_checker.UnhealthyComponents(),
            std::set<std::string>({
                "kubernetes_pod_thread",
                "kubernetes_node_thread",
            }));
}

TEST_F(HealthCheckerUnittest, FailurePersists) {
  Configuration config;
  MetadataStore store(config);
  HealthChecker health_checker(config, store);
  EXPECT_TRUE(health_checker.UnhealthyComponents().empty());
  SetUnhealthy(&health_checker, "kubernetes_pod_thread");
  EXPECT_EQ(health_checker.UnhealthyComponents(),
            std::set<std::string>({
                "kubernetes_pod_thread",
            }));
  SetUnhealthy(&health_checker, "kubernetes_pod_thread");
  EXPECT_EQ(health_checker.UnhealthyComponents(),
            std::set<std::string>({
                "kubernetes_pod_thread",
            }));
}

TEST_F(HealthCheckerUnittest, RecentMetadataSucceeds) {
  Configuration config(std::istringstream(
      "HealthCheckMaxDataAgeSeconds: 20"
  ));
  MetadataStore store(config);
  HealthChecker health_checker(config, store);
  EXPECT_TRUE(health_checker.UnhealthyComponents().empty());
  time_point now = std::chrono::system_clock::now();
  time_point then = now - std::chrono::seconds(10);
  store.UpdateMetadata(
      MonitoredResource("my_resource", {}),
      MetadataStore::Metadata("0", false, then, then, json::object({})));
  EXPECT_TRUE(health_checker.UnhealthyComponents().empty());
}

TEST_F(HealthCheckerUnittest, StaleMetadataCausesFailure) {
  Configuration config(std::istringstream(
      "HealthCheckMaxDataAgeSeconds: 1"
  ));
  MetadataStore store(config);
  HealthChecker health_checker(config, store);
  EXPECT_TRUE(health_checker.UnhealthyComponents().empty());
  time_point now = std::chrono::system_clock::now();
  time_point then = now - std::chrono::seconds(10);
  store.UpdateMetadata(
      MonitoredResource("my_resource", {}),
      MetadataStore::Metadata("0", false, then, then, json::object({})));
  EXPECT_EQ(health_checker.UnhealthyComponents(),
            std::set<std::string>({
                "my_resource",
            }));
}

TEST_F(HealthCheckerUnittest, UpdatedMetadataClearsFailure) {
  Configuration config(std::istringstream(
      "HealthCheckMaxDataAgeSeconds: 1"
  ));
  MetadataStore store(config);
  HealthChecker health_checker(config, store);
  EXPECT_TRUE(health_checker.UnhealthyComponents().empty());
  time_point now = std::chrono::system_clock::now();
  time_point then = now - std::chrono::seconds(10);
  store.UpdateMetadata(
      MonitoredResource("my_resource", {}),
      MetadataStore::Metadata("0", false, then, then, json::object({})));
  EXPECT_EQ(health_checker.UnhealthyComponents(),
            std::set<std::string>({
                "my_resource",
            }));
  now = std::chrono::system_clock::now();
  store.UpdateMetadata(
      MonitoredResource("my_resource", {}),
      MetadataStore::Metadata("0", false, now, now, json::object({})));
  EXPECT_TRUE(health_checker.UnhealthyComponents().empty());
}

}  // namespace google
