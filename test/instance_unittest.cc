#include "../src/configuration.h"
#include "../src/environment.h"
#include "../src/instance.h"
#include "../src/resource.h"
#include "../src/updater.h"
#include "gtest/gtest.h"

namespace google {

namespace {

class InstanceTest : public ::testing::Test {
 protected:
  static const MetadataStore::Metadata& GetResourceMetadata(
      const MetadataUpdater::ResourceMetadata& rm) {
    return rm.metadata;
  }

  static MonitoredResource GetMonitoredResource(
      const MetadataUpdater::ResourceMetadata& rm) {
    return rm.resource;
  }

  static std::vector<std::string> GetResourceIds(
      const MetadataUpdater::ResourceMetadata& rm) {
    return rm.ids;
  }
};

TEST_F(InstanceTest, GetInstanceMonitoredResource) {
  Configuration config(std::istringstream(
      "InstanceResourceType: gce_instance\n"
      "InstanceId: 1234567891011\n"
      "InstanceZone: us-east1-b\n"
  ));
  Environment env(config);
  MonitoredResource mr = InstanceReader::InstanceResource(env);
  EXPECT_EQ(MonitoredResource("gce_instance", {
    {"instance_id", "1234567891011"},
    {"zone", "us-east1-b"}
  }), mr);
}

TEST_F(InstanceTest, GetInstanceMetatadataQuery) {
  Configuration config(std::istringstream(
      "InstanceResourceType: gce_instance\n"
      "InstanceId: 1234567891011\n"
      "InstanceZone: us-east1-b\n"
  ));
  InstanceReader reader(config);
  const auto result = reader.MetadataQuery();
  const std::vector<std::string> ids_exp({"", "1234567891011"});
  EXPECT_EQ(1, result.size());
  const MetadataUpdater::ResourceMetadata& rm = result[0];
  EXPECT_EQ(MonitoredResource("gce_instance", {
      {"instance_id", "1234567891011"},
      {"zone", "us-east1-b"}
  }), GetMonitoredResource(rm));
  EXPECT_EQ(ids_exp, GetResourceIds(rm));
  EXPECT_EQ(MetadataStore::Metadata::IGNORED().ignore,
            GetResourceMetadata(rm).ignore);
}

}  // namespace

}  // google
