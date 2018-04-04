#include "../src/health_checker.h"
#include "gtest/gtest.h"
#include <stdio.h>
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

using HealthCheckerDeathTest = HealthCheckerUnittest;

TEST_F(HealthCheckerUnittest, DefaultHealthy) {
  Configuration config(std::istringstream("HealthCheckFile: './" +
                                          std::string(test_info_->name()) +
                                          "/unhealthy'\n"));
  HealthChecker health_checker(config);
  EXPECT_TRUE(IsHealthy(health_checker));
  Cleanup(&health_checker);
}

TEST_F(HealthCheckerUnittest, SimpleFailure) {
  Configuration config(std::istringstream("HealthCheckFile: './" +
                                          std::string(test_info_->name()) +
                                          "/unhealthy'\n"));
  HealthChecker health_checker(config);
  SetUnhealthy(&health_checker, "kubernetes_pod_thread");
  EXPECT_FALSE(IsHealthy(health_checker));
  Cleanup(&health_checker);
}

TEST_F(HealthCheckerUnittest, MultiFailure) {
  Configuration config(std::istringstream("HealthCheckFile: './" +
                                          std::string(test_info_->name()) +
                                          "/unhealthy'\n"));
  HealthChecker health_checker(config);
  EXPECT_TRUE(IsHealthy(health_checker));
  SetUnhealthy(&health_checker, "kubernetes_pod_thread");
  EXPECT_FALSE(IsHealthy(health_checker));
  SetUnhealthy(&health_checker, "kubernetes_node_thread");
  EXPECT_FALSE(IsHealthy(health_checker));
  Cleanup(&health_checker);
}

TEST_F(HealthCheckerUnittest, FailurePersists) {
  Configuration config(std::istringstream("HealthCheckFile: './" +
                                          std::string(test_info_->name()) +
                                          "/unhealthy'\n"));
  HealthChecker health_checker(config);
  EXPECT_TRUE(IsHealthy(health_checker));
  SetUnhealthy(&health_checker, "kubernetes_pod_thread");
  EXPECT_FALSE(IsHealthy(health_checker));
  SetUnhealthy(&health_checker, "kubernetes_pod_thread");
  EXPECT_FALSE(IsHealthy(health_checker));
  Cleanup(&health_checker);
}

TEST_F(HealthCheckerDeathTest, Exit) {
  Configuration config(std::istringstream("HealthCheckFile: './" +
                                          std::string(test_info_->name()) +
                                          "/unhealthy'\n"
                                          "KillAgentOnFailure: true\n"));
  HealthChecker health_checker(config);
  EXPECT_EXIT(SetUnhealthy(&health_checker, "kubernetes_pod_thread"),
              ::testing::ExitedWithCode(EXIT_FAILURE), "");
  Cleanup(&health_checker);
}
}  // namespace google
