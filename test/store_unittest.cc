#include "../src/configuration.h"
#include "../src/resource.h"
#include "../src/store.h"
#include "../src/time.h"
#include "gtest/gtest.h"

namespace google {

class MetadataStoreTest : public ::testing::Test {
 protected:
  MetadataStoreTest() : config(), store(config) {}

  std::map<MonitoredResource, MetadataStore::Metadata> GetMetadataMap() const {
    return store.GetMetadataMap();
  }

  void PurgeDeletedEntries() {
    store.PurgeDeletedEntries();
  }

  Configuration config;
  MetadataStore store;
};

TEST_F(MetadataStoreTest, ResourceWithOneIdCorrectlyStored) {
  MonitoredResource mr("type", {});
  store.UpdateResource({"id"}, mr);
  EXPECT_EQ(mr, store.LookupResource("id"));
}

TEST_F(MetadataStoreTest, EmptyStoreLookupThrowsError) {
  EXPECT_THROW(store.LookupResource("missing_id"), std::out_of_range);
}

TEST_F(MetadataStoreTest, ResourceLookupFailuresAreIndependent) {
  MonitoredResource mr("type", {});
  store.UpdateResource({"id"}, mr);
  EXPECT_THROW(store.LookupResource("missing_id"), std::out_of_range);
  EXPECT_EQ(mr, store.LookupResource("id"));
}

TEST_F(MetadataStoreTest, MultipleResourcesWithSingleIdsCorrectlyStored) {
  MonitoredResource mr1("type1", {});
  MonitoredResource mr2("type2", {});
  store.UpdateResource({"id1"}, mr1);
  store.UpdateResource({"id2"}, mr2);
  EXPECT_EQ(mr2, store.LookupResource("id2"));
  EXPECT_EQ(mr1, store.LookupResource("id1"));
}

TEST_F(MetadataStoreTest, SingleResourceWithMultipleIdsCorrectlyStored) {
  MonitoredResource mr("type", {});
  store.UpdateResource({"id1", "id2", "id3"}, mr);
  EXPECT_EQ(mr, store.LookupResource("id3"));
  EXPECT_EQ(mr, store.LookupResource("id1"));
  EXPECT_EQ(mr, store.LookupResource("id2"));
}

TEST_F(MetadataStoreTest, MultipleResourcesAndIdsCorrectlyStored) {
  MonitoredResource mr1("type1", {});
  MonitoredResource mr2("type2", {});
  store.UpdateResource({"id1", "id2", "id3"}, mr1);
  store.UpdateResource({"id-a", "id-b", "id-c"}, mr2);
  EXPECT_EQ(mr1, store.LookupResource("id1"));
  EXPECT_EQ(mr1, store.LookupResource("id2"));
  EXPECT_EQ(mr1, store.LookupResource("id3"));
  EXPECT_EQ(mr2, store.LookupResource("id-a"));
  EXPECT_EQ(mr2, store.LookupResource("id-b"));
  EXPECT_EQ(mr2, store.LookupResource("id-c"));
}

TEST_F(MetadataStoreTest, ResourceToIdsAssociationCorrectlyUpdated) {
  MonitoredResource mr("type", {});
  store.UpdateResource({"id1", "id2"}, mr);
  EXPECT_EQ(mr, store.LookupResource("id1"));
  EXPECT_EQ(mr, store.LookupResource("id2"));
  store.UpdateResource({"id-a", "id-b"}, mr);
  EXPECT_EQ(mr, store.LookupResource("id1"));
  EXPECT_EQ(mr, store.LookupResource("id2"));
  EXPECT_EQ(mr, store.LookupResource("id-a"));
  EXPECT_EQ(mr, store.LookupResource("id-b"));
}

TEST_F(MetadataStoreTest, DefaultMetadataMapIsEmpty) {
  const auto metadata_map = GetMetadataMap();
  EXPECT_TRUE(metadata_map.empty());
}

TEST_F(MetadataStoreTest, UpdateResourceDoesNotUpdateMetadata) {
  MonitoredResource mr("type", {});
  store.UpdateResource({"id1"}, mr);
  const auto metadata_map = GetMetadataMap();
  EXPECT_TRUE(metadata_map.empty());
}

TEST_F(MetadataStoreTest, UpdateMetadataChangesMetadataMap) {
  MonitoredResource mr("type", {});
  MetadataStore::Metadata m(
      "default-version",
      false,
      std::chrono::system_clock::now(),
      std::chrono::system_clock::now(),
      json::object({{"f", json::string("hello")}}));
  store.UpdateMetadata(mr, std::move(m));
  const auto metadata_map = GetMetadataMap();
  EXPECT_EQ(1, metadata_map.size());
  EXPECT_EQ("default-version", metadata_map.at(mr).version);
}

TEST_F(MetadataStoreTest, MultipleUpdateMetadataChangesMetadataMap) {
  MonitoredResource mr1("type1", {});
  MonitoredResource mr2("type2", {});
  MetadataStore::Metadata m1(
      "default-version1",
      false,
      std::chrono::system_clock::now(),
      std::chrono::system_clock::now(),
      json::object({{"f", json::string("hello")}}));
  MetadataStore::Metadata m2(
      "default-version2",
      false,
      std::chrono::system_clock::now(),
      std::chrono::system_clock::now(),
      json::object({{"f", json::string("hello")}}));
  store.UpdateMetadata(mr1, std::move(m1));
  store.UpdateMetadata(mr2, std::move(m2));
  const auto metadata_map = GetMetadataMap();
  EXPECT_EQ(2, metadata_map.size());
  EXPECT_EQ("default-version1", metadata_map.at(mr1).version);
  EXPECT_EQ("default-version2", metadata_map.at(mr2).version);
}

TEST_F(MetadataStoreTest, UpdateMetadataForResourceChangesMetadataEntry) {
  MonitoredResource mr("type", {});
  MetadataStore::Metadata m1(
      "default-version1",
      false,
      std::chrono::system_clock::now(),
      std::chrono::system_clock::now(),
      json::object({{"f", json::string("hello")}}));
  store.UpdateMetadata(mr, std::move(m1));
  const auto metadata_map_before = GetMetadataMap();
  EXPECT_EQ(1, metadata_map_before.size());
  EXPECT_EQ("default-version1", metadata_map_before.at(mr).version);
  MetadataStore::Metadata m2(
      "default-version2",
      false,
      std::chrono::system_clock::now(),
      std::chrono::system_clock::now(),
      json::object({{"f", json::string("hello")}}));
  store.UpdateMetadata(mr, std::move(m2));
  const auto metadata_map_after = GetMetadataMap();
  EXPECT_EQ(1, metadata_map_after.size());
  EXPECT_EQ("default-version2", metadata_map_after.at(mr).version);
}

TEST_F(MetadataStoreTest, PurgeDeletedEntriesDeletesCorrectMetadata) {
  MonitoredResource mr1("type1", {});
  MonitoredResource mr2("type2", {});
  MetadataStore::Metadata m1(
      "default-version1",
      false,
      std::chrono::system_clock::now(),
      std::chrono::system_clock::now(),
      json::object({{"f", json::string("hello")}}));
  MetadataStore::Metadata m2(
      "default-version2",
      true,
      std::chrono::system_clock::now(),
      std::chrono::system_clock::now(),
      json::object({{"f", json::string("hello")}}));
  store.UpdateMetadata(mr1, std::move(m1));
  store.UpdateMetadata(mr2, std::move(m2));
  const auto metadata_map_before = GetMetadataMap();
  EXPECT_EQ(2, metadata_map_before.size());
  EXPECT_EQ("default-version1", metadata_map_before.at(mr1).version);
  EXPECT_EQ("default-version2", metadata_map_before.at(mr2).version);
  PurgeDeletedEntries();
  const auto metadata_map_after = GetMetadataMap();
  EXPECT_EQ(1, metadata_map_after.size());
  EXPECT_EQ("default-version1", metadata_map_after.at(mr1).version);
  EXPECT_THROW(metadata_map_after.at(mr2), std::out_of_range);
}

TEST(MetadataTest, MetadataCorrectlyConstructed) {
  MetadataStore::Metadata m(
      "default-version",
      false,
      time::rfc3339::FromString("2018-03-03T01:23:45.678901234Z"),
      time::rfc3339::FromString("2018-03-03T01:32:45.678901234Z"),
      json::object({{"f", json::string("hello")}}));
  EXPECT_FALSE(m.ignore);
  EXPECT_EQ("default-version", m.version);
  EXPECT_FALSE(m.is_deleted);
  EXPECT_EQ(time::rfc3339::FromString("2018-03-03T01:23:45.678901234Z"),
            m.created_at);
  EXPECT_EQ(time::rfc3339::FromString("2018-03-03T01:32:45.678901234Z"),
            m.collected_at);
  EXPECT_EQ("{\"f\":\"hello\"}", m.metadata->ToString());
}

TEST(MetadataTest, MetadataCorrectlyCloned) {
  MetadataStore::Metadata m(
      "default-version",
      false,
      time::rfc3339::FromString("2018-03-03T01:23:45.678901234Z"),
      time::rfc3339::FromString("2018-03-03T01:32:45.678901234Z"),
      json::object({{"f", json::string("hello")}}));
  MetadataStore::Metadata m_clone = m.Clone();
  EXPECT_FALSE(m_clone.ignore);
  EXPECT_EQ(m.version, m_clone.version);
  EXPECT_FALSE(m_clone.is_deleted);
  EXPECT_EQ(m.created_at, m_clone.created_at);
  EXPECT_EQ(m.collected_at, m_clone.collected_at);
  EXPECT_EQ(m.metadata->ToString(), m_clone.metadata->ToString());
}

TEST(MetadataTest, IgnoredMetadataCorrectlyCreated) {
  MetadataStore::Metadata m = MetadataStore::Metadata::IGNORED();
  EXPECT_TRUE(m.ignore);
}

TEST(MetadataTest, IgnoredMetadataCorrectlyCloned) {
  MetadataStore::Metadata m = MetadataStore::Metadata::IGNORED();
  MetadataStore::Metadata m_clone = m.Clone();
  EXPECT_EQ(m.ignore, m_clone.ignore);
}

}  // namespace
