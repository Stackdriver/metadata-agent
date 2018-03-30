#include "../src/configuration.h"
#include "../src/resource.h"
#include "../src/store.h"
#include "../src/updater.h"
#include "gtest/gtest.h"

#include <string>
#include <vector>

namespace google {

class UpdaterTest : public ::testing::Test {
 protected:
  // query_metadata function not needed to test callbacks.
  UpdaterTest() : config(), store(config), updater(
      config, &store, "Test", 60.0, nullptr) {}

  std::map<MonitoredResource, MetadataStore::Metadata> GetMetadataMap() const {
    return store.GetMetadataMap();
  }

  bool ValidateConfiguration() const {
    return updater.ValidateConfiguration();
  }

  void UpdateMetadataCallback(MetadataUpdater::ResourceMetadata& result) {
    updater.UpdateMetadataCallback(std::move(result));
  }

  void UpdateResourceCallback(MetadataUpdater::ResourceMetadata& result) {
    updater.UpdateResourceCallback(std::move(result));
  }

  Configuration config;
  MetadataStore store;
  PollingMetadataUpdater updater;
};

namespace {

TEST_F(UpdaterTest, ValidateConfiguration) {
  EXPECT_TRUE(ValidateConfiguration());
}

TEST_F(UpdaterTest, UpdateMetadataCallbackTest) {
  MetadataStore::Metadata m(
      "test-version",
      false,
      std::chrono::system_clock::now(),
      std::chrono::system_clock::now(),
      json::object({{"f", json::string("test")}}));
  MonitoredResource resource("test_resource", {});
  MetadataUpdater::ResourceMetadata metadata(
      std::vector<std::string>({"", "test-prefix"}),
      resource, std::move(m));
  UpdateMetadataCallback(metadata);
  const auto metadata_map = GetMetadataMap();
  EXPECT_EQ(1, metadata_map.size());
  EXPECT_EQ("test-version", metadata_map.at(resource).version);
  EXPECT_EQ("{\"f\":\"test\"}", metadata_map.at(resource).metadata->ToString());
}

TEST_F(UpdaterTest, UpdateResourceCallbackTest) {
  MetadataUpdater::ResourceMetadata metadata(
      std::vector<std::string>({"", "test-prefix"}),
      MonitoredResource("test_resource", {}),
      MetadataStore::Metadata::IGNORED()
  );
  UpdateResourceCallback(metadata);
  EXPECT_EQ(MonitoredResource("test_resource", {}), store.LookupResource(""));
  EXPECT_EQ(MonitoredResource("test_resource", {}),
            store.LookupResource("test-prefix"));
}

}  // namespace

}  // google
