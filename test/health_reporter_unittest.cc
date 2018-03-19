#include "../src/health_reporter.h"
#include "gtest/gtest.h"
#include <stdio.h>
#include <sstream>

namespace google {

class HealthReporterUnittest : public ::testing::Test {
 public:
  HealthReporterUnittest() {}
  ~HealthReporterUnittest() {
    if(prefix.length() > 0) {
      std::ostringstream oss;
      oss << prefix << "_metadata_agent_unhealthy";
      std::remove(oss.str().c_str());

      oss.str("");
      oss.clear();
      oss << prefix << "_node_callback";
      std::remove(oss.str().c_str());

      oss.str("");
      oss.clear();
      oss << prefix << "_watch_pods";
      std::remove(oss.str().c_str());
    }
  }
 protected:
  void SetUnhealthy(HealthReporter healthReporter, const std::string &state_name) {
    healthReporter.SetUnhealthy(state_name);
  }

  bool ReportHealth(HealthReporter healthReporter) {
    return healthReporter.ReportHealth();
  }

  bool GetTotalHealthState(HealthReporter healthReporter) {
    return healthReporter.GetTotalHealthState();
  }

  std::string prefix;
};

TEST_F(HealthReporterUnittest, DefaultHealthy) {
  prefix = "DefaultHealthy";
  HealthReporter healthReporter(prefix);
  EXPECT_EQ(true, GetTotalHealthState(healthReporter));
}

TEST_F(HealthReporterUnittest, SimpleFailure) {
  prefix = "SimpleFailure";
  HealthReporter healthReporter(prefix);
  SetUnhealthy(healthReporter, "watch_pods");
  EXPECT_EQ(false, GetTotalHealthState(healthReporter));
}

TEST_F(HealthReporterUnittest, MultiFailure) {
  prefix = "MultiFailure";
  HealthReporter healthReporter(prefix);
  EXPECT_EQ(true, GetTotalHealthState(healthReporter));
  SetUnhealthy(healthReporter, "watch_pods");
  EXPECT_EQ(false, GetTotalHealthState(healthReporter));
  SetUnhealthy(healthReporter, "node_callback");
  EXPECT_EQ(false, GetTotalHealthState(healthReporter));
}

TEST_F(HealthReporterUnittest, NoRecovery) {
  prefix = "NoRecovery";
  HealthReporter healthReporter(prefix);
  EXPECT_EQ(true, GetTotalHealthState(healthReporter));
  SetUnhealthy(healthReporter, "watch_pods");
  EXPECT_EQ(false, GetTotalHealthState(healthReporter));
  SetUnhealthy(healthReporter, "watch_pods");
  EXPECT_EQ(false, GetTotalHealthState(healthReporter));
}
}  // namespace google
