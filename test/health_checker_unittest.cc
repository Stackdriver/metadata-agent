#include "../src/health_checker.h"
#include "../src/store.h"
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

}  // namespace google
