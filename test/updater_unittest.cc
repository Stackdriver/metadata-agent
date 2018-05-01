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
  UpdaterTest() : config(), store(config) {}

  static void ValidateConfiguration(MetadataUpdater* updater) {
    updater->ValidateConfiguration();
  }

  static bool ShouldStartUpdater(MetadataUpdater* updater) {
    return updater->ShouldStartUpdater();
  }

  static void UpdateMetadataCallback(
      MetadataUpdater* updater,
      MetadataUpdater::ResourceMetadata&& result) {
    updater->UpdateMetadataCallback(std::move(result));
  }

  static void UpdateResourceCallback(
      MetadataUpdater* updater,
      const MetadataUpdater::ResourceMetadata& result) {
    updater->UpdateResourceCallback(result);
  }

  Configuration config;
  MetadataStore store;
};

namespace {

TEST_F(UpdaterTest, ValidateConfiguration_OneMinutePollingIntervalIsValid) {
  PollingMetadataUpdater updater(config, &store, "Test", 60, nullptr);
  EXPECT_NO_THROW(ValidateConfiguration(&updater));
}

TEST_F(UpdaterTest, ValidateConfiguration_ZeroSecondPollingIntervalIsValid) {
  PollingMetadataUpdater updater(config, &store, "Test", 0, nullptr);
  EXPECT_NO_THROW(ValidateConfiguration(&updater));
}

TEST_F(UpdaterTest, ValidateConfiguration_NegativePollingIntervalIsInvalid) {
  PollingMetadataUpdater updater(config, &store, "BadUpdater", -1, nullptr);
  EXPECT_THROW(
      ValidateConfiguration(&updater),
      MetadataUpdater::ConfigurationValidationError);
}

TEST_F(UpdaterTest, ShouldStart_OneMinutePollingIntervalEnablesUpdate) {
  PollingMetadataUpdater updater(config, &store, "Test", 60, nullptr);
  EXPECT_TRUE(ShouldStartUpdater(&updater));
}

TEST_F(UpdaterTest, ShouldStart_ZeroSecondPollingIntervalDisablesUpdate) {
  PollingMetadataUpdater updater(config, &store, "Test", 0, nullptr);
  EXPECT_FALSE(ShouldStartUpdater(&updater));
}

TEST_F(UpdaterTest, UpdateMetadataCallback) {
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
  PollingMetadataUpdater updater(config, &store, "Test", 60, nullptr);
  UpdateMetadataCallback(&updater, std::move(metadata));
  const auto metadata_map = store.GetMetadataMap();
  EXPECT_EQ(1, metadata_map.size());
  EXPECT_EQ("test-version", metadata_map.at(resource).version);
  EXPECT_EQ("{\"f\":\"test\"}", metadata_map.at(resource).metadata->ToString());
}

TEST_F(UpdaterTest, UpdateResourceCallback) {
  MetadataUpdater::ResourceMetadata metadata(
      std::vector<std::string>({"", "test-prefix"}),
      MonitoredResource("test_resource", {}),
      MetadataStore::Metadata::IGNORED()
  );
  PollingMetadataUpdater updater(config, &store, "Test", 60, nullptr);
  UpdateResourceCallback(&updater, metadata);
  EXPECT_EQ(MonitoredResource("test_resource", {}),
            store.LookupResource(""));
  EXPECT_EQ(MonitoredResource("test_resource", {}),
            store.LookupResource("test-prefix"));
}

}  // namespace

}  // google
