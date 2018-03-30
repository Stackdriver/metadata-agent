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
  MonitoredResource resource("type", {});
  store.UpdateResource({"id"}, resource);
  EXPECT_EQ(resource, store.LookupResource("id"));
}

TEST_F(MetadataStoreTest, EmptyStoreLookupThrowsError) {
  EXPECT_THROW(store.LookupResource("missing_id"), std::out_of_range);
}

TEST_F(MetadataStoreTest, ResourceLookupFailuresAreIndependent) {
  MonitoredResource resource("type", {});
  store.UpdateResource({"id"}, resource);
  EXPECT_THROW(store.LookupResource("missing_id"), std::out_of_range);
  EXPECT_EQ(resource, store.LookupResource("id"));
}

TEST_F(MetadataStoreTest, MultipleResourcesWithSingleIdsCorrectlyStored) {
  MonitoredResource resource1("type1", {});
  MonitoredResource resource2("type2", {});
  store.UpdateResource({"id1"}, resource1);
  store.UpdateResource({"id2"}, resource2);
  EXPECT_EQ(resource2, store.LookupResource("id2"));
  EXPECT_EQ(resource1, store.LookupResource("id1"));
}

TEST_F(MetadataStoreTest, SingleResourceWithMultipleIdsCorrectlyStored) {
  MonitoredResource resource("type", {});
  store.UpdateResource({"id1", "id2", "id3"}, resource);
  EXPECT_EQ(resource, store.LookupResource("id3"));
  EXPECT_EQ(resource, store.LookupResource("id1"));
  EXPECT_EQ(resource, store.LookupResource("id2"));
}

TEST_F(MetadataStoreTest, MultipleResourcesAndIdsCorrectlyStored) {
  MonitoredResource resource1("type1", {});
  MonitoredResource resource2("type2", {});
  store.UpdateResource({"id1", "id2", "id3"}, resource1);
  store.UpdateResource({"id-a", "id-b", "id-c"}, resource2);
  EXPECT_EQ(resource1, store.LookupResource("id1"));
  EXPECT_EQ(resource1, store.LookupResource("id2"));
  EXPECT_EQ(resource1, store.LookupResource("id3"));
  EXPECT_EQ(resource2, store.LookupResource("id-a"));
  EXPECT_EQ(resource2, store.LookupResource("id-b"));
  EXPECT_EQ(resource2, store.LookupResource("id-c"));
}

TEST_F(MetadataStoreTest, ResourceToIdsAssociationCorrectlyUpdated) {
  MonitoredResource resource("type", {});
  store.UpdateResource({"id1", "id2"}, resource);
  EXPECT_EQ(resource, store.LookupResource("id1"));
  EXPECT_EQ(resource, store.LookupResource("id2"));
  store.UpdateResource({"id-a", "id-b"}, resource);
  EXPECT_EQ(resource, store.LookupResource("id1"));
  EXPECT_EQ(resource, store.LookupResource("id2"));
  EXPECT_EQ(resource, store.LookupResource("id-a"));
  EXPECT_EQ(resource, store.LookupResource("id-b"));
}

TEST_F(MetadataStoreTest, DefaultMetadataMapIsEmpty) {
  const auto metadata_map = GetMetadataMap();
  EXPECT_TRUE(metadata_map.empty());
}

TEST_F(MetadataStoreTest, UpdateResourceDoesNotUpdateMetadata) {
  MonitoredResource resource("type", {});
  store.UpdateResource({"id1"}, resource);
  const auto metadata_map = GetMetadataMap();
  EXPECT_TRUE(metadata_map.empty());
}

TEST_F(MetadataStoreTest, UpdateMetadataChangesMetadataMap) {
  MonitoredResource resource("type", {});
  MetadataStore::Metadata m(
      "default-version",
      false,
      std::chrono::system_clock::now(),
      std::chrono::system_clock::now(),
      json::object({{"f", json::string("hello")}}));
  store.UpdateMetadata(resource, std::move(m));
  const auto metadata_map = GetMetadataMap();
  EXPECT_EQ(1, metadata_map.size());
  EXPECT_EQ("default-version", metadata_map.at(resource).version);
}

TEST_F(MetadataStoreTest, MultipleUpdateMetadataChangesMetadataMap) {
  MonitoredResource resource1("type1", {});
  MonitoredResource resource2("type2", {});
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
  store.UpdateMetadata(resource1, std::move(m1));
  store.UpdateMetadata(resource2, std::move(m2));
  const auto metadata_map = GetMetadataMap();
  EXPECT_EQ(2, metadata_map.size());
  EXPECT_EQ("default-version1", metadata_map.at(resource1).version);
  EXPECT_EQ("default-version2", metadata_map.at(resource2).version);
}

TEST_F(MetadataStoreTest, UpdateMetadataForResourceChangesMetadataEntry) {
  MonitoredResource resource("type", {});
  MetadataStore::Metadata m1(
      "default-version1",
      false,
      std::chrono::system_clock::now(),
      std::chrono::system_clock::now(),
      json::object({{"f", json::string("hello")}}));
  store.UpdateMetadata(resource, std::move(m1));
  const auto metadata_map_before = GetMetadataMap();
  EXPECT_EQ(1, metadata_map_before.size());
  EXPECT_EQ("default-version1", metadata_map_before.at(resource).version);
  MetadataStore::Metadata m2(
      "default-version2",
      false,
      std::chrono::system_clock::now(),
      std::chrono::system_clock::now(),
      json::object({{"f", json::string("hello")}}));
  store.UpdateMetadata(resource, std::move(m2));
  const auto metadata_map_after = GetMetadataMap();
  EXPECT_EQ(1, metadata_map_after.size());
  EXPECT_EQ("default-version2", metadata_map_after.at(resource).version);
}

TEST_F(MetadataStoreTest, PurgeDeletedEntriesDeletesCorrectMetadata) {
  MonitoredResource resource1("type1", {});
  MonitoredResource resource2("type2", {});
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
  store.UpdateMetadata(resource1, std::move(m1));
  store.UpdateMetadata(resource2, std::move(m2));
  const auto metadata_map_before = GetMetadataMap();
  EXPECT_EQ(2, metadata_map_before.size());
  EXPECT_EQ("default-version1", metadata_map_before.at(resource1).version);
  EXPECT_EQ("default-version2", metadata_map_before.at(resource2).version);
  PurgeDeletedEntries();
  const auto metadata_map_after = GetMetadataMap();
  EXPECT_EQ(1, metadata_map_after.size());
  EXPECT_EQ("default-version1", metadata_map_after.at(resource1).version);
  EXPECT_THROW(metadata_map_after.at(resource2), std::out_of_range);
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
