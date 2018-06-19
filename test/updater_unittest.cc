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

class FakeClock {
 public:
  // Requirements for Clock, see http://en.cppreference.com/w/cpp/named_req/Clock
  typedef uint64_t rep;
  typedef std::ratio<1l, 1000000000l> period;
  typedef std::chrono::duration<rep, period> duration;
  typedef std::chrono::time_point<FakeClock> time_point;
  static const bool is_steady;
  static time_point now() noexcept;

  static void Advance(duration d) noexcept;

 private:
  FakeClock() = delete;
  ~FakeClock() = delete;
  FakeClock(FakeClock const&) = delete;

  static time_point now_;
};

FakeClock::time_point FakeClock::now_;

const bool FakeClock::is_steady = false;

FakeClock::time_point FakeClock::now() noexcept {
  return now_;
}

void FakeClock::Advance(duration d) noexcept {
  now_ += d;
}

namespace {

class ValidateStaticConfigurationTest : public UpdaterTest {};

TEST_F(ValidateStaticConfigurationTest, OneMinutePollingIntervalIsValid) {
  PollingMetadataUpdater<> updater(config, &store, "Test", 60, nullptr);
  EXPECT_NO_THROW(ValidateStaticConfiguration(&updater));
}

TEST_F(ValidateStaticConfigurationTest, ZeroSecondPollingIntervalIsValid) {
  PollingMetadataUpdater<> updater(config, &store, "Test", 0, nullptr);
  EXPECT_NO_THROW(ValidateStaticConfiguration(&updater));
}

TEST_F(ValidateStaticConfigurationTest, NegativePollingIntervalIsInvalid) {
  PollingMetadataUpdater<> updater(config, &store, "BadUpdater", -1, nullptr);
  EXPECT_THROW(
      ValidateStaticConfiguration(&updater),
      MetadataUpdater::ConfigurationValidationError);
}

class ShouldStartUpdaterTest : public UpdaterTest {};

TEST_F(ShouldStartUpdaterTest, OneMinutePollingIntervalEnablesUpdate) {
  PollingMetadataUpdater<> updater(config, &store, "Test", 60, nullptr);
  EXPECT_TRUE(ShouldStartUpdater(&updater));
}

TEST_F(ShouldStartUpdaterTest, ZeroSecondPollingIntervalDisablesUpdate) {
  PollingMetadataUpdater<> updater(config, &store, "Test", 0, nullptr);
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
  void StopUpdater() {}

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
  EXPECT_THROW(updater.start(), MetadataUpdater::ConfigurationValidationError);
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
  EXPECT_NO_THROW(updater.start());
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
  EXPECT_THROW(updater.start(), MetadataUpdater::ConfigurationValidationError);
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
  EXPECT_NO_THROW(updater.start());
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
      "test-version",
      false,
      std::chrono::system_clock::now(),
      std::chrono::system_clock::now(),
      json::object({{"f", json::string("test")}}));
  MonitoredResource resource("test_resource", {});
  MetadataUpdater::ResourceMetadata metadata(
      std::vector<std::string>({"", "test-prefix"}),
      resource, std::move(m));
  PollingMetadataUpdater<> updater(config, &store, "Test", 60, nullptr);
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
  PollingMetadataUpdater<> updater(config, &store, "Test", 60, nullptr);
  UpdateResourceCallback(&updater, metadata);
  EXPECT_EQ(MonitoredResource("test_resource", {}),
            store.LookupResource(""));
  EXPECT_EQ(MonitoredResource("test_resource", {}),
            store.LookupResource("test-prefix"));
}

bool WaitForResource(const MetadataStore& store,
                     const MonitoredResource& resource) {
  for (int i = 0; i < 30; i++){
    const auto metadata_map = store.GetMetadataMap();
    if (metadata_map.find(resource) != metadata_map.end()) {
      return true;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
  return false;
}

TEST_F(UpdaterTest, PollingMetadataUpdater) {
  // Start updater with 1 second polling interval, using fake clock
  // implementation.
  //
  // Each callback will return a new resource "test_resource_<i>".
  int i = 0;
  PollingMetadataUpdater<FakeClock> updater(
    config, &store, "Test", 1,
    [&i]() {
      std::vector<MetadataUpdater::ResourceMetadata> result;
      result.emplace_back(std::move(MetadataUpdater::ResourceMetadata(
        {"", "test-prefix"},
        MonitoredResource("test_resource_" + std::to_string(i++), {}),
        MetadataStore::Metadata::IGNORED())));
      return result;
    });
  std::thread updater_thread([&updater] { updater.start(); });

  // Wait for update, verify store, and advance fake clock.
  EXPECT_TRUE(WaitForResource(store, MonitoredResource("test_resource_0", {})));
  EXPECT_EQ(1, store.GetMetadataMap().size());
  FakeClock::Advance(std::chrono::seconds(1));

  // Repeat.
  EXPECT_TRUE(WaitForResource(store, MonitoredResource("test_resource_1", {})));
  EXPECT_EQ(2, store.GetMetadataMap().size());
  FakeClock::Advance(std::chrono::seconds(1));

  // Repeat.
  EXPECT_TRUE(WaitForResource(store, MonitoredResource("test_resource_2", {})));
  EXPECT_EQ(3, store.GetMetadataMap().size());

  updater.stop();
  updater_thread.join();
}

}  // namespace

}  // google
