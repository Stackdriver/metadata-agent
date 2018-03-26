#include "../src/health_checker.h"
#include "gtest/gtest.h"
#include <stdio.h>
#include <sstream>
#include <boost/filesystem.hpp>


namespace google {

class HealthCheckerUnittest : public ::testing::Test {
 protected:
  ~HealthCheckerUnittest() {
    if (isolation_path_.length() > 0) {
      std::ostringstream oss;
      oss << isolation_path_ << "/unhealthy";
      std::remove(oss.str().c_str());
      std::remove(isolation_path_.c_str());
    }
  }

  static void SetUnhealthy(HealthChecker* health_checker,
                           const std::string& state_name) {
    health_checker->SetUnhealthy(state_name);
  }

  static bool IsHealthy(HealthChecker& health_checker) {
    return health_checker.IsHealthy();
  }

  void SetIsolationPath(const std::string& isolation_path) {
    isolation_path_ = isolation_path;
    std::stringstream stream(
        "HealthCheckFile: './" + isolation_path_ + "/unhealthy'");
    config_.ParseConfiguration(stream);
    boost::filesystem::create_directory(isolation_path_);
  }

  std::string isolation_path_;
  MetadataAgentConfiguration config_;
};

TEST_F(HealthCheckerUnittest, DefaultHealthy) {
  SetIsolationPath(test_info_->name());
  HealthChecker healthChecker(config_);
  EXPECT_TRUE(IsHealthy(healthChecker));
}

TEST_F(HealthCheckerUnittest, SimpleFailure) {
  SetIsolationPath(test_info_->name());
  HealthChecker healthChecker(config_);
  SetUnhealthy(&healthChecker, "kubernetes_pod_thread");
  EXPECT_FALSE(IsHealthy(healthChecker));
}

TEST_F(HealthCheckerUnittest, MultiFailure) {
  SetIsolationPath(test_info_->name());
  HealthChecker healthChecker(config_);
  EXPECT_TRUE(IsHealthy(healthChecker));
  SetUnhealthy(&healthChecker, "kubernetes_pod_thread");
  EXPECT_FALSE(IsHealthy(healthChecker));
  SetUnhealthy(&healthChecker, "kubernetes_node_thread");
  EXPECT_FALSE(IsHealthy(healthChecker));
}

TEST_F(HealthCheckerUnittest, NoRecovery) {
  SetIsolationPath(test_info_->name());
  HealthChecker healthChecker(config_);
  EXPECT_TRUE(IsHealthy(healthChecker));
  SetUnhealthy(&healthChecker, "kubernetes_pod_thread");
  EXPECT_FALSE(IsHealthy(healthChecker));
  SetUnhealthy(&healthChecker, "kubernetes_pod_thread");
  EXPECT_FALSE(IsHealthy(healthChecker));
}
}  // namespace google
