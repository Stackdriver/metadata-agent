#include "../src/configuration.h"
#include "../src/resource.h"
#include "../src/store.h"
#include "../src/time.h"
#include "gtest/gtest.h"

namespace google {

class MetadataAgentStoreTest : public ::testing::Test {
 protected:
  Configuration *config;
  MetadataStore* store;

  void SetupStore() {
    config = new Configuration(std::istringstream(""));
    store = new MetadataStore(*config);
  }

  std::map<MonitoredResource, MetadataStore::Metadata> GetMetadataMap() {
    return store->GetMetadataMap();
  }

  void PurgeDeletedEntries() {
    store->PurgeDeletedEntries();
  }

  virtual void TearDown() {
    delete config;
    delete store;
  }

};

TEST_F(MetadataAgentStoreTest, ResourceIDStore) {
  SetupStore();
  MonitoredResource mr("some_resource", {});
  std::vector<std::string> resourceId(1, "id");
  store->UpdateResource(resourceId, mr);
  EXPECT_EQ("some_resource", store->LookupResource("id").type());
}

TEST_F(MetadataAgentStoreTest, ResourceNotFoundStore) {
  SetupStore();
  EXPECT_THROW(store->LookupResource("some_resource_id").type(),
               std::out_of_range);
}

TEST_F(MetadataAgentStoreTest, ManyResourcesStore) {
  SetupStore();
  MonitoredResource mr1("some_resource1", {});
  MonitoredResource mr2("some_resource2", {});
  std::vector<std::string> resourceId1(1, "id1");
  std::vector<std::string> resourceId2(1, "id2");
  store->UpdateResource(resourceId1, mr1);
  store->UpdateResource(resourceId2, mr2);
  EXPECT_EQ("some_resource1", store->LookupResource("id1").type());
  EXPECT_EQ("some_resource2", store->LookupResource("id2").type());
}

TEST_F(MetadataAgentStoreTest, ManyResourceIDsStore) {
  SetupStore();
  MonitoredResource mr("some_resource", {});
  const char* ids_arr[] = {"id1", "id2", "id3"};
  std::vector<std::string> resourceIds(ids_arr, std::end(ids_arr));
  store->UpdateResource(resourceIds, mr);
  EXPECT_EQ("some_resource", store->LookupResource("id1").type());
  EXPECT_EQ("some_resource", store->LookupResource("id2").type());
  EXPECT_EQ("some_resource", store->LookupResource("id3").type());
}

TEST_F(MetadataAgentStoreTest, ManyIdsAndResourcesStore) {
  SetupStore();
  MonitoredResource mr1("some_resource1", {});
  MonitoredResource mr2("some_resource2", {});
  const char* ids_arr1[] = {"id1", "id2", "id3"};
  const char* ids_arr2[] = {"id-a", "id-b", "id-c"};
  std::vector<std::string> resourceIds1(ids_arr1, std::end(ids_arr1));
  store->UpdateResource(resourceIds1, mr1);
  std::vector<std::string> resourceIds2(ids_arr2, std::end(ids_arr2));
  store->UpdateResource(resourceIds2, mr2);
  EXPECT_EQ("some_resource1", store->LookupResource("id1").type());
  EXPECT_EQ("some_resource1", store->LookupResource("id2").type());
  EXPECT_EQ("some_resource1", store->LookupResource("id3").type());
  EXPECT_EQ("some_resource2", store->LookupResource("id-a").type());
  EXPECT_EQ("some_resource2", store->LookupResource("id-b").type());
  EXPECT_EQ("some_resource2", store->LookupResource("id-c").type());
}

TEST_F(MetadataAgentStoreTest, MultipleResourceIdsUpdateStore) {
  SetupStore();
  MonitoredResource mr("some_resource", {});
  const char* ids_arr[] = {"id1", "id2"};
  std::vector<std::string> resourceIds(ids_arr, std::end(ids_arr));
  store->UpdateResource(resourceIds, mr);
  EXPECT_EQ("some_resource", store->LookupResource("id1").type());
  EXPECT_EQ("some_resource", store->LookupResource("id2").type());
  const char* ids_arr2[] = {"id-a", "id-b"};
  std::vector<std::string> resourceIds2(ids_arr2, std::end(ids_arr2));
  store->UpdateResource(resourceIds2, mr);
  EXPECT_EQ("some_resource", store->LookupResource("id-a").type());
  EXPECT_EQ("some_resource", store->LookupResource("id-b").type());
}

TEST_F(MetadataAgentStoreTest, EmptyMetadataStore) {
  SetupStore();
  std::map<MonitoredResource, MetadataStore::Metadata> metadata_map =
      GetMetadataMap();
  EXPECT_TRUE(metadata_map.empty());
}

TEST_F(MetadataAgentStoreTest, EmptyMetadataNonEmptyResourceStore) {
  SetupStore();
  MonitoredResource mr("some_resource", {});
  std::vector<std::string> resourceId(1, "abc123");
  store->UpdateResource(resourceId, mr);
  std::map<MonitoredResource, MetadataStore::Metadata> metadata_map =
      GetMetadataMap();
  EXPECT_TRUE(metadata_map.empty());
}

TEST_F(MetadataAgentStoreTest, OneMetadataEntryStore) {
  SetupStore();
  MonitoredResource mr("some_resource", {});
  MetadataStore::Metadata m(
      "default-version",
      false,
      time::rfc3339::FromString("2018-03-03T01:23:45.678901234Z"),
      time::rfc3339::FromString("2018-03-03T01:32:45.678901234Z"),
      json::Parser::FromString("{\"f\":\"hello\"}")
      );
  store->UpdateMetadata(mr, std::move(m));
  std::map<MonitoredResource, MetadataStore::Metadata> metadata_map =
      GetMetadataMap();
  EXPECT_FALSE(metadata_map.empty());
  EXPECT_EQ("default-version", metadata_map.at(mr).version);
}

TEST_F(MetadataAgentStoreTest, MultipleMetadataEntriesStore) {
  SetupStore();
  MonitoredResource mr1("some_resource1", {});
  MonitoredResource mr2("some_resource2", {});
  MetadataStore::Metadata m1(
      "default-version1",
      false,
      time::rfc3339::FromString("2018-03-03T01:23:45.678901234Z"),
      time::rfc3339::FromString("2018-03-03T01:32:45.678901234Z"),
      json::Parser::FromString("{\"f\":\"hello\"}")
      );
  MetadataStore::Metadata m2(
      "default-version2",
      false,
      time::rfc3339::FromString("2018-03-03T01:23:45.678901234Z"),
      time::rfc3339::FromString("2018-03-03T01:32:45.678901234Z"),
      json::Parser::FromString("{\"f\":\"hello\"}")
      );
  store->UpdateMetadata(mr1, std::move(m1));
  store->UpdateMetadata(mr2, std::move(m2));
  std::map<MonitoredResource, MetadataStore::Metadata> metadata_map =
      GetMetadataMap();
  EXPECT_FALSE(metadata_map.empty());
  EXPECT_EQ("default-version1", metadata_map.at(mr1).version);
  EXPECT_EQ("default-version2", metadata_map.at(mr2).version);
}

TEST_F(MetadataAgentStoreTest, UpdateMetadataEntryStore) {
  SetupStore();
  MonitoredResource mr("some_resource", {});
  MetadataStore::Metadata m1(
      "default-version1",
      false,
      time::rfc3339::FromString("2018-03-03T01:23:45.678901234Z"),
      time::rfc3339::FromString("2018-03-03T01:32:45.678901234Z"),
      json::Parser::FromString("{\"f\":\"hello\"}")
      );
  store->UpdateMetadata(mr, std::move(m1));
  std::map<MonitoredResource, MetadataStore::Metadata> metadata_map =
      GetMetadataMap();
  EXPECT_FALSE(metadata_map.empty());
  EXPECT_EQ("default-version1", metadata_map.at(mr).version);
  MetadataStore::Metadata m2(
      "default-version2",
      false,
      time::rfc3339::FromString("2018-03-03T01:23:45.678901234Z"),
      time::rfc3339::FromString("2018-03-03T01:32:45.678901234Z"),
      json::Parser::FromString("{\"f\":\"hello\"}")
      );
  store->UpdateMetadata(mr, std::move(m2));
  metadata_map = GetMetadataMap();
  EXPECT_EQ("default-version2", metadata_map.at(mr).version);
}

TEST_F(MetadataAgentStoreTest, PurgeDeletedEntriesStore) {
  SetupStore();
  MonitoredResource mr1("some_resource1", {});
  MonitoredResource mr2("some_resource2", {});
  MetadataStore::Metadata m1(
      "default-version1",
      false,
      time::rfc3339::FromString("2018-03-03T01:23:45.678901234Z"),
      time::rfc3339::FromString("2018-03-03T01:32:45.678901234Z"),
      json::Parser::FromString("{\"f\":\"hello\"}")
      );
  MetadataStore::Metadata m2(
      "default-version2",
      true,
      time::rfc3339::FromString("2018-03-03T01:23:45.678901234Z"),
      time::rfc3339::FromString("2018-03-03T01:32:45.678901234Z"),
      json::Parser::FromString("{\"f\":\"hello\"}")
      );
  store->UpdateMetadata(mr1, std::move(m1));
  store->UpdateMetadata(mr2, std::move(m2));
  std::map<MonitoredResource, MetadataStore::Metadata> metadata_map =
      GetMetadataMap();
  EXPECT_FALSE(metadata_map.empty());
  EXPECT_EQ("default-version1", metadata_map.at(mr1).version);
  EXPECT_EQ("default-version2", metadata_map.at(mr2).version);
  PurgeDeletedEntries();
  metadata_map = GetMetadataMap();
  EXPECT_EQ("default-version1", metadata_map.at(mr1).version);
  EXPECT_THROW(metadata_map.at(mr2), std::out_of_range);
}

TEST(MetadataTest, ParseMetadata) {
  MetadataStore::Metadata m(
      "default-version",
      false,
      time::rfc3339::FromString("2018-03-03T01:23:45.678901234Z"),
      time::rfc3339::FromString("2018-03-03T01:32:45.678901234Z"),
      json::Parser::FromString("{\"f\":\"hello\"}")
      );
  EXPECT_EQ("default-version", m.version);
  EXPECT_FALSE(m.is_deleted);
  EXPECT_FALSE(m.ignore);
  EXPECT_EQ(time::rfc3339::FromString("2018-03-03T01:23:45.678901234Z"),
            m.created_at);
  EXPECT_EQ(time::rfc3339::FromString("2018-03-03T01:32:45.678901234Z"),
            m.collected_at);
  const auto& obj = m.metadata->As<json::Object>();
  EXPECT_TRUE(obj->Has("f"));
  EXPECT_EQ("hello", obj->Get<json::String>("f"));
}

TEST(MetadataTest, CloneMetadata) {
  MetadataStore::Metadata m(
      "default-version",
      false,
      time::rfc3339::FromString("2018-03-03T01:23:45.678901234Z"),
      time::rfc3339::FromString("2018-03-03T01:32:45.678901234Z"),
      json::Parser::FromString("{\"f\":\"hello\"}")
      );
  MetadataStore::Metadata m_clone = m.Clone();
  EXPECT_EQ("default-version", m_clone.version);
  EXPECT_FALSE(m_clone.is_deleted);
  EXPECT_EQ(time::rfc3339::FromString("2018-03-03T01:23:45.678901234Z"),
            m_clone.created_at);
  EXPECT_EQ(time::rfc3339::FromString("2018-03-03T01:32:45.678901234Z"),
            m_clone.collected_at);
  const auto& obj = m_clone.metadata->As<json::Object>();
  EXPECT_TRUE(obj->Has("f"));
  EXPECT_EQ("hello", obj->Get<json::String>("f"));
}

TEST(MetadataTest, TestIgnoredMetadata) {
  MetadataStore::Metadata m = MetadataStore::Metadata::IGNORED();
  EXPECT_EQ("", m.version);
  EXPECT_EQ(time::rfc3339::FromString("0000-00-00T00:00:00.000000000Z"),
            m.created_at);
  EXPECT_EQ(time::rfc3339::FromString("0000-00-00T00:00:00.000000000Z"),
            m.collected_at);
  EXPECT_EQ("{}", m.metadata->ToString());
  EXPECT_FALSE(m.is_deleted);
  EXPECT_TRUE(m.ignore);
}

TEST(MetadataTest, TestClonedIgnoredMetadata) {
  MetadataStore::Metadata m = MetadataStore::Metadata::IGNORED();
  MetadataStore::Metadata m_clone = m.Clone();
  EXPECT_EQ("", m_clone.version);
  EXPECT_EQ(time::rfc3339::FromString("0000-00-00T00:00:00.000000000Z"),
            m_clone.created_at);
  EXPECT_EQ(time::rfc3339::FromString("0000-00-00T00:00:00.000000000Z"),
            m_clone.collected_at);
  EXPECT_EQ("{}", m_clone.metadata->ToString());
  EXPECT_FALSE(m_clone.is_deleted);
  EXPECT_TRUE(m_clone.ignore);
}

}  // namespace
