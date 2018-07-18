/*
 * Copyright 2018 Google Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 **/

#include "../src/configuration.h"
#include "../src/resource.h"
#include "../src/store.h"
#include "../src/time.h"
#include "gtest/gtest.h"

namespace google {

class MetadataStoreTest : public ::testing::Test {
 protected:
  MetadataStoreTest() : config(), store(config) {}

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

TEST_F(MetadataStoreTest, DefaultMetadataIsEmpty) {
  const auto metadata = store.GetMetadata();
  EXPECT_TRUE(metadata.empty());
}

TEST_F(MetadataStoreTest, UpdateResourceDoesNotUpdateMetadata) {
  MonitoredResource resource("type", {});
  store.UpdateResource({"id1"}, resource);
  const auto metadata = store.GetMetadata();
  EXPECT_TRUE(metadata.empty());
}

TEST_F(MetadataStoreTest, UpdateMetadataChangesMetadata) {
  MetadataStore::Metadata m(
      "default-name",
      "default-type",
      "default-location",
      "default-version",
      "default-schema",
      false,
      std::chrono::system_clock::now(),
      json::object({{"f", json::string("hello")}}));
  store.UpdateMetadata(std::move(m));
  const auto metadata = store.GetMetadata();
  EXPECT_EQ(1, metadata.size());
  EXPECT_EQ("default-name", metadata[0].name);
  EXPECT_EQ("default-type", metadata[0].type);
  EXPECT_EQ("default-location", metadata[0].location);
  EXPECT_EQ("default-version", metadata[0].version);
  EXPECT_EQ("default-schema", metadata[0].schema_name);
}

TEST_F(MetadataStoreTest, MultipleUpdateMetadataChangesMetadata) {
  MetadataStore::Metadata m1(
      "default-name1",
      "default-type1",
      "default-location1",
      "default-version1",
      "default-schema1",
      false,
      std::chrono::system_clock::now(),
      json::object({{"f", json::string("hello")}}));
  MetadataStore::Metadata m2(
      "default-name2",
      "default-type2",
      "default-location2",
      "default-version2",
      "default-schema2",
      false,
      std::chrono::system_clock::now(),
      json::object({{"f", json::string("hello")}}));
  store.UpdateMetadata(std::move(m1));
  store.UpdateMetadata(std::move(m2));
  const auto metadata = store.GetMetadata();
  EXPECT_EQ(2, metadata.size());
  EXPECT_EQ("default-type1", metadata[0].type);
  EXPECT_EQ("default-type2", metadata[1].type);
}

TEST_F(MetadataStoreTest, UpdateMetadataForResourceChangesMetadataEntry) {
  MetadataStore::Metadata m1(
      "name",
      "default-type1",
      "default-location1",
      "version",
      "default-schema1",
      false,
      std::chrono::system_clock::now(),
      json::object({{"f", json::string("hello")}}));
  store.UpdateMetadata(std::move(m1));
  const auto metadata_before = store.GetMetadata();
  EXPECT_EQ(1, metadata_before.size());
  EXPECT_EQ("default-type1", metadata_before[0].type);
  MetadataStore::Metadata m2(
      "name",
      "default-type2",
      "default-location2",
      "version",
      "default-schema2",
      false,
      std::chrono::system_clock::now(),
      json::object({{"f", json::string("hello")}}));
  store.UpdateMetadata(std::move(m2));
  const auto metadata_after = store.GetMetadata();
  EXPECT_EQ(1, metadata_after.size());
  EXPECT_EQ("default-type2", metadata_after[0].type);
}

TEST_F(MetadataStoreTest, PurgeDeletedEntriesDeletesCorrectMetadata) {
  MetadataStore::Metadata m1(
      "default-name1",
      "default-type1",
      "default-location1",
      "default-version1",
      "default-schema1",
      false,
      std::chrono::system_clock::now(),
      json::object({{"f", json::string("hello")}}));
  MetadataStore::Metadata m2(
      "default-name2",
      "default-type2",
      "default-location2",
      "default-version2",
      "default-schema2",
      true,
      std::chrono::system_clock::now(),
      json::object({{"f", json::string("hello")}}));
  store.UpdateMetadata(std::move(m1));
  store.UpdateMetadata(std::move(m2));
  const auto metadata_before = store.GetMetadata();
  EXPECT_EQ(2, metadata_before.size());
  EXPECT_EQ("default-type1", metadata_before[0].type);
  EXPECT_EQ("default-type2", metadata_before[1].type);
  PurgeDeletedEntries();
  const auto metadata_after = store.GetMetadata();
  EXPECT_EQ(1, metadata_after.size());
  EXPECT_EQ("default-type1", metadata_after[0].type);
}

TEST(MetadataTest, MetadataCorrectlyConstructed) {
  MetadataStore::Metadata m(
      "default-name",
      "default-type",
      "default-location",
      "default-version",
      "default-schema",
      false,
      time::rfc3339::FromString("2018-03-03T01:32:45.678901234Z"),
      json::object({{"f", json::string("hello")}}));
  EXPECT_FALSE(m.ignore);
  EXPECT_EQ("default-type", m.type);
  EXPECT_FALSE(m.is_deleted);
  EXPECT_EQ(time::rfc3339::FromString("2018-03-03T01:32:45.678901234Z"),
            m.collected_at);
  EXPECT_EQ("{\"f\":\"hello\"}", m.metadata->ToString());
}

TEST(MetadataTest, MetadataCorrectlyCloned) {
  MetadataStore::Metadata m(
      "default-name",
      "default-type",
      "default-location",
      "default-version",
      "default-schema",
      false,
      time::rfc3339::FromString("2018-03-03T01:23:45.678901234Z"),
      json::object({{"f", json::string("hello")}}));
  MetadataStore::Metadata m_clone = m.Clone();
  EXPECT_FALSE(m_clone.ignore);
  EXPECT_EQ(m.type, m_clone.type);
  EXPECT_FALSE(m_clone.is_deleted);
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
