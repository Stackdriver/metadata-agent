#include "../src/configuration.h"
#include "../src/environment.h"
#include "../src/instance.h"
#include "../src/resource.h"
#include "../src/updater.h"
#include "gtest/gtest.h"

namespace google {

namespace {

TEST(InstanceTest, GetInstanceMonitoredResource) {
  Configuration config(std::istringstream(
      "InstanceResourceType: gce_instance\n"
      "InstanceId: 1234567891011\n"
      "InstanceZone: us-east1-b\n"
  ));
  Environment env(config);
  EXPECT_EQ(MonitoredResource("gce_instance", {
    {"instance_id", "1234567891011"},
    {"zone", "us-east1-b"}
  }), InstanceReader::InstanceResource(env));
}

TEST(InstanceTest, GetInstanceMetatadataQuery) {
  Configuration config(std::istringstream(
      "InstanceResourceType: gce_instance\n"
      "InstanceId: 1234567891011\n"
      "InstanceZone: us-east1-b\n"
  ));
  InstanceReader reader(config);
  const auto result = reader.MetadataQuery();
  const std::vector<std::string> ids_expected({"", "1234567891011"});
  EXPECT_EQ(1, result.size());
  const MetadataUpdater::ResourceMetadata& rm = result[0];
  EXPECT_EQ(MonitoredResource("gce_instance", {
    {"instance_id", "1234567891011"},
    {"zone", "us-east1-b"}
  }), rm.resource());
  EXPECT_EQ(ids_expected, rm.ids());
  EXPECT_TRUE(rm.metadata().ignore);
}

}  // namespace

}  // google
