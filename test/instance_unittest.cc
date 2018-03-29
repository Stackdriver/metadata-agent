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
  const MetadataStore::Metadata&& GetResourceMetadata(
      const MetadataUpdater::ResourceMetadata* rm) {
    return std::move(rm->metadata);
  }

  MonitoredResource GetMonitoredResource(
      const MetadataUpdater::ResourceMetadata* rm) {
    return rm->resource;
  }

  std::vector<std::string> GetResourceIds(
      const MetadataUpdater::ResourceMetadata* rm) {
    return rm->ids;
  }
};

TEST_F(InstanceTest, GetInstanceMonitoredResource) {
  Configuration config(std::istringstream(
      "InstanceResourceType: gce_instance\n"
      "InstanceId: 1234567891011\n"
      "InstanceZone: us-east1-b\n"
  ));
  Environment env(config);
  MonitoredResource mr_actual =
      InstanceReader::InstanceResource(env);
  MonitoredResource mr_expected(
      "gce_instance",
      {{"instance_id", "1234567891011"}, {"zone", "us-east1-b"}});
  EXPECT_EQ(mr_expected, mr_actual);
}

TEST_F(InstanceTest, GetInstanceMonitoredResourceWithEmptyConfig) {
  Environment env(*(new Configuration()));
  MonitoredResource mr_actual =
      InstanceReader::InstanceResource(env);
  MonitoredResource mr_expected(
      "gce_instance", {{"instance_id", ""}, {"zone", ""}});
  EXPECT_EQ(mr_expected, mr_actual);
}

TEST_F(InstanceTest, GetInstanceMetatadataQuery) {
  Configuration config(std::istringstream(
      "InstanceResourceType: gce_instance\n"
      "InstanceId: 1234567891011\n"
      "InstanceZone: us-east1-b\n"
  ));
  InstanceReader reader(config);
  const auto& result = reader.MetadataQuery();
  MonitoredResource mr_expected(
      "gce_instance",
      {{"instance_id", "1234567891011"}, {"zone", "us-east1-b"}});
  std::string vinit[] = {"", "1234567891011"};
  std::vector<std::string> ids_exp = {vinit, std::end(vinit)};
  EXPECT_EQ(1, result.size());
  const MetadataUpdater::ResourceMetadata& rm = result.at(0);
  EXPECT_EQ(mr_expected, GetMonitoredResource(&rm));
  EXPECT_EQ(ids_exp, GetResourceIds(&rm));
  EXPECT_EQ(MetadataStore::Metadata::IGNORED().ignore,
            GetResourceMetadata(&rm).ignore);
}

}  // namespace

}  // google
