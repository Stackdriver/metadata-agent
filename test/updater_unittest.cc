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

  static void ValidateStaticConfiguration(MetadataUpdater* updater) {
    updater->ValidateStaticConfiguration();
  }

  static bool ShouldStartUpdater(MetadataUpdater* updater) {
    return updater->ShouldStartUpdater();
  }

  static void ValidateDynamicConfiguration(MetadataUpdater* updater) {
    updater->ValidateDynamicConfiguration();
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

class ValidateStaticConfigurationTest : public UpdaterTest {};

TEST_F(ValidateStaticConfigurationTest, OneMinutePollingIntervalIsValid) {
  PollingMetadataUpdater updater(config, &store, "Test", 60, nullptr);
  EXPECT_NO_THROW(ValidateStaticConfiguration(&updater));
}

TEST_F(ValidateStaticConfigurationTest, ZeroSecondPollingIntervalIsValid) {
  PollingMetadataUpdater updater(config, &store, "Test", 0, nullptr);
  EXPECT_NO_THROW(ValidateStaticConfiguration(&updater));
}

TEST_F(ValidateStaticConfigurationTest, NegativePollingIntervalIsInvalid) {
  PollingMetadataUpdater updater(config, &store, "BadUpdater", -1, nullptr);
  EXPECT_THROW(
      ValidateStaticConfiguration(&updater),
      MetadataUpdater::ConfigurationValidationError);
}

class ShouldStartUpdaterTest : public UpdaterTest {};

TEST_F(ShouldStartUpdaterTest, OneMinutePollingIntervalEnablesUpdate) {
  PollingMetadataUpdater updater(config, &store, "Test", 60, nullptr);
  EXPECT_TRUE(ShouldStartUpdater(&updater));
}

TEST_F(ShouldStartUpdaterTest, ZeroSecondPollingIntervalDisablesUpdate) {
  PollingMetadataUpdater updater(config, &store, "Test", 0, nullptr);
  EXPECT_FALSE(ShouldStartUpdater(&updater));
}

class ValidationOrderingTest : public UpdaterTest {};

class MockMetadataUpdater : public MetadataUpdater {
 public:
  MockMetadataUpdater(const Configuration& config, bool fail_static, bool should_start, bool fail_dynamic)
      : MetadataUpdater(config, nullptr, "MOCK"),
        fail_static_validation(fail_static),
        should_start_updater(should_start),
        fail_dynamic_validation(fail_dynamic) {}

  const std::vector<std::string>& call_sequence() { return call_sequence_; }

 protected:
  void ValidateStaticConfiguration() const
      throw(ConfigurationValidationError) {
    call_sequence_.push_back("ValidateStaticConfiguration");
    if (fail_static_validation) {
      throw ConfigurationValidationError("ValidateStaticConfiguration");
    }
  }
  bool ShouldStartUpdater() const {
    call_sequence_.push_back("ShouldStartUpdater");
    return should_start_updater;
  }
  void ValidateDynamicConfiguration() const
      throw(ConfigurationValidationError) {
    call_sequence_.push_back("ValidateDynamicConfiguration");
    if (fail_dynamic_validation) {
      throw ConfigurationValidationError("ValidateDynamicConfiguration");
    }
  }
  void StartUpdater() {
    call_sequence_.push_back("StartUpdater");
  }
  void NotifyStopUpdater() {}

  mutable std::vector<std::string> call_sequence_;

 private:
  bool fail_static_validation;
  bool should_start_updater;
  bool fail_dynamic_validation;
};

TEST_F(ValidationOrderingTest, FailedStaticCheckStopsOtherChecks) {
  MockMetadataUpdater updater(
      config,
      /*fail_static=*/true,
      /*should_start=*/true,
      /*fail_dynamic=*/true);
  EXPECT_THROW(updater.Start(), MetadataUpdater::ConfigurationValidationError);
  EXPECT_EQ(
      std::vector<std::string>({
          "ValidateStaticConfiguration",
      }),
      updater.call_sequence());
}

TEST_F(ValidationOrderingTest, FalseShouldStartUpdaterStopsDynamicChecks) {
  MockMetadataUpdater updater(
      config,
      /*fail_static=*/false,
      /*should_start=*/false,
      /*fail_dynamic=*/false);
  EXPECT_NO_THROW(updater.Start());
  EXPECT_EQ(
      std::vector<std::string>({
          "ValidateStaticConfiguration",
          "ShouldStartUpdater",
      }),
      updater.call_sequence());
}

TEST_F(ValidationOrderingTest, FailedDynamicCheckStopsStartUpdater) {
  MockMetadataUpdater updater(
      config,
      /*fail_static=*/false,
      /*should_start=*/true,
      /*fail_dynamic=*/true);
  EXPECT_THROW(updater.Start(), MetadataUpdater::ConfigurationValidationError);
  EXPECT_EQ(
      std::vector<std::string>({
          "ValidateStaticConfiguration",
          "ShouldStartUpdater",
          "ValidateDynamicConfiguration",
      }),
      updater.call_sequence());
}

TEST_F(ValidationOrderingTest, AllChecksPassedInvokesStartUpdater) {
  MockMetadataUpdater updater(
      config,
      /*fail_static=*/false,
      /*should_start=*/true,
      /*fail_dynamic=*/false);
  EXPECT_NO_THROW(updater.Start());
  EXPECT_EQ(
      std::vector<std::string>({
          "ValidateStaticConfiguration",
          "ShouldStartUpdater",
          "ValidateDynamicConfiguration",
          "StartUpdater",
      }),
      updater.call_sequence());
}


TEST_F(UpdaterTest, UpdateMetadataCallback) {
  MetadataStore::Metadata m(
      "test-type",
      "test-location",
      "test-version",
      "test-schema-name",
      false,
      std::chrono::system_clock::now(),
      json::object({{"f", json::string("test")}}));
  MonitoredResource resource("test_resource", {});
  const std::string frn = "/test";
  MetadataUpdater::ResourceMetadata metadata(
      std::vector<std::string>({"", "test-prefix"}),
      resource, frn, std::move(m));
  PollingMetadataUpdater updater(config, &store, "Test", 60, nullptr);
  UpdateMetadataCallback(&updater, std::move(metadata));
  const auto metadata_map = store.GetMetadataMap();
  EXPECT_EQ(1, metadata_map.size());
  EXPECT_EQ("test-version", metadata_map.at(frn).version);
  EXPECT_EQ("{\"f\":\"test\"}", metadata_map.at(frn).metadata->ToString());
}

TEST_F(UpdaterTest, UpdateResourceCallback) {
  MetadataUpdater::ResourceMetadata metadata(
      std::vector<std::string>({"", "test-prefix"}),
      MonitoredResource("test_resource", {}),
      "/test",
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
