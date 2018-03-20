#include "../src/health_checker.h"
#include "gtest/gtest.h"
#include <stdio.h>
#include <sstream>

namespace google {

class HealthCheckerUnittest : public ::testing::Test {
 public:
  HealthCheckerUnittest() {
    std::stringstream stream("HealthCheckLocation: ''");
    config_.ParseConfiguration(stream);
  }

  ~HealthCheckerUnittest() {
    if (prefix_.length() > 0) {
      std::ostringstream oss;
      oss << prefix_ << "_metadata_agent_unhealthy";
      std::remove(oss.str().c_str());

      oss.str("");
      oss.clear();
      oss << prefix_ << "_kubernetes_node_thread";
      std::remove(oss.str().c_str());

      oss.str("");
      oss.clear();
      oss << prefix_ << "_kubernetes_pod_thread";
      std::remove(oss.str().c_str());
    }
  }
 protected:
  void SetUnhealthy(HealthChecker healthReporter, const std::string &state_name) {
    healthReporter.SetUnhealthy(state_name);
  }

  bool ReportHealth(HealthChecker healthReporter) {
    return healthReporter.ReportHealth();
  }

  bool IsHealthy(HealthChecker healthReporter) {
    return healthReporter.IsHealthy();
  }

  std::string prefix_;
  MetadataAgentConfiguration config_;
};

TEST_F(HealthCheckerUnittest, DefaultHealthy) {
  prefix_ = "DefaultHealthy";
  HealthChecker healthReporter(prefix_, config_);
  EXPECT_TRUE(IsHealthy(healthReporter));
}

TEST_F(HealthCheckerUnittest, SimpleFailure) {
  prefix_ = "SimpleFailure";
  HealthChecker healthReporter(prefix_, config_);
  SetUnhealthy(healthReporter, "kubernetes_pod_thread");
  EXPECT_FALSE(IsHealthy(healthReporter));
}

TEST_F(HealthCheckerUnittest, MultiFailure) {
  prefix_ = "MultiFailure";
  HealthChecker healthReporter(prefix_, config_);
  EXPECT_TRUE(IsHealthy(healthReporter));
  SetUnhealthy(healthReporter, "kubernetes_pod_thread");
  EXPECT_FALSE(IsHealthy(healthReporter));
  SetUnhealthy(healthReporter, "kubernetes_node_thread");
  EXPECT_FALSE(IsHealthy(healthReporter));
}

TEST_F(HealthCheckerUnittest, NoRecovery) {
  prefix_ = "NoRecovery";
  HealthChecker healthReporter(prefix_, config_);
  EXPECT_TRUE(IsHealthy(healthReporter));
  SetUnhealthy(healthReporter, "kubernetes_pod_thread");
  EXPECT_FALSE(IsHealthy(healthReporter));
  SetUnhealthy(healthReporter, "kubernetes_pod_thread");
  EXPECT_FALSE(IsHealthy(healthReporter));
}
}  // namespace google
