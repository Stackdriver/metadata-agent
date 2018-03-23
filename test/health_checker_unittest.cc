#include "../src/health_checker.h"
#include "gtest/gtest.h"
#include <stdio.h>
#include <sstream>
#include <boost/filesystem.hpp>


namespace google {

class HealthCheckerUnittest : public ::testing::Test {
 public:
  HealthCheckerUnittest() { }

  ~HealthCheckerUnittest() {
    if (isolation_path_.length() > 0) {
      std::ostringstream oss;
      oss << isolation_path_ << "/metadata_agent_unhealthy";
      std::remove(oss.str().c_str());

      oss.str("");
      oss.clear();
      oss << isolation_path_ << "/kubernetes_node_thread";
      std::remove(oss.str().c_str());

      oss.str("");
      oss.clear();
      oss << isolation_path_ << "/kubernetes_pod_thread";
      std::remove(oss.str().c_str());

      std::remove(isolation_path_.c_str());
    }
  }
 protected:
  static void SetUnhealthyStateName(HealthChecker* healthChecker,
                           const std::string& state_name) {
    healthChecker->SetUnhealthyStateName(state_name);
  }

  static bool ReportHealth(HealthChecker* healthChecker) {
    return healthChecker->ReportHealth();
  }

  static bool IsHealthy(const HealthChecker& healthChecker) {
    return healthChecker.IsHealthy();
  }

  void SetIsolationPath(const std::string& isolation_path) {
    isolation_path_ = isolation_path;
    std::stringstream stream(
        "HealthCheckFileDirectory: '" + isolation_path_ + "'");
    config_.ParseConfiguration(stream);
    boost::filesystem::create_directory(isolation_path_);
  }

  std::string isolation_path_;
  MetadataAgentConfiguration config_;
};

TEST_F(HealthCheckerUnittest, DefaultHealthy) {
  SetIsolationPath("DefaultHealthy");
  HealthChecker healthChecker(config_);
  EXPECT_TRUE(IsHealthy(healthChecker));
}

TEST_F(HealthCheckerUnittest, SimpleFailure) {
  SetIsolationPath("SimpleFailure");
  HealthChecker healthChecker(config_);
  SetUnhealthyStateName(&healthChecker, "kubernetes_pod_thread");
  EXPECT_FALSE(IsHealthy(healthChecker));
}

TEST_F(HealthCheckerUnittest, MultiFailure) {
  SetIsolationPath("MultiFailure");
  HealthChecker healthChecker(config_);
  EXPECT_TRUE(IsHealthy(healthChecker));
  SetUnhealthyStateName(&healthChecker, "kubernetes_pod_thread");
  EXPECT_FALSE(IsHealthy(healthChecker));
  SetUnhealthyStateName(&healthChecker, "kubernetes_node_thread");
  EXPECT_FALSE(IsHealthy(healthChecker));
}

TEST_F(HealthCheckerUnittest, NoRecovery) {
  SetIsolationPath("NoRecovery");
  HealthChecker healthChecker(config_);
  EXPECT_TRUE(IsHealthy(healthChecker));
  SetUnhealthyStateName(&healthChecker, "kubernetes_pod_thread");
  EXPECT_FALSE(IsHealthy(healthChecker));
  SetUnhealthyStateName(&healthChecker, "kubernetes_pod_thread");
  EXPECT_FALSE(IsHealthy(healthChecker));
}
}  // namespace google
