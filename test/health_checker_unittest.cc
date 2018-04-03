#include "../src/health_checker.h"
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
  HealthChecker health_checker(config);
  EXPECT_TRUE(IsHealthy(health_checker));
  Cleanup(&health_checker);
}

TEST_F(HealthCheckerUnittest, SimpleFailure) {
  Configuration config(IsolationPathConfig(test_info_->name()));
  HealthChecker health_checker(config);
  SetUnhealthy(&health_checker, "kubernetes_pod_thread");
  EXPECT_FALSE(IsHealthy(health_checker));
  Cleanup(&health_checker);
}

TEST_F(HealthCheckerUnittest, MultiFailure) {
  Configuration config(IsolationPathConfig(test_info_->name()));
  HealthChecker health_checker(config);
  EXPECT_TRUE(IsHealthy(health_checker));
  SetUnhealthy(&health_checker, "kubernetes_pod_thread");
  EXPECT_FALSE(IsHealthy(health_checker));
  SetUnhealthy(&health_checker, "kubernetes_node_thread");
  EXPECT_FALSE(IsHealthy(health_checker));
  Cleanup(&health_checker);
}

TEST_F(HealthCheckerUnittest, FailurePersists) {
  Configuration config(IsolationPathConfig(test_info_->name()));
  HealthChecker health_checker(config);
  EXPECT_TRUE(IsHealthy(health_checker));
  SetUnhealthy(&health_checker, "kubernetes_pod_thread");
  EXPECT_FALSE(IsHealthy(health_checker));
  SetUnhealthy(&health_checker, "kubernetes_pod_thread");
  EXPECT_FALSE(IsHealthy(health_checker));
  Cleanup(&health_checker);
}
}  // namespace google
