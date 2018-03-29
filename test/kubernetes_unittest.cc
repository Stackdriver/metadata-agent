#include "../src/configuration.h"
#include "../src/instance.h"
#include "../src/kubernetes.h"
#include "../src/updater.h"
#include "gtest/gtest.h"

namespace google {

class KubernetesTest : public ::testing::Test {
  protected:
   MetadataUpdater::ResourceMetadata GetNodeMetadata(
       const KubernetesReader& reader, const json::Object *node,
       Timestamp collected_at, bool is_deleted) const
       throw(json::Exception) {
      return reader.GetNodeMetadata(node, collected_at, is_deleted);
   }
};

TEST_F(KubernetesTest, GetNodeMetadata) {
  Configuration config(std::stringstream(
    "KubernetesClusterName: TestClusterName\n"
    "KubernetesClusterLocation: TestClusterLocation\n"
    "MetadataIngestionRawContentVersion: TestVersion\n"
    "InstanceZone: TestZone\n"
    "InstanceId: TestID\n"
  ));
  Environment environment(config);
  KubernetesReader reader(config, nullptr);
  json::value node = json::object({
    {"metadata", json::object({
      {"name", json::string("testname")},
      {"creationTimestamp", json::string("2018-03-03T01:23:45.678901234Z")},
    })}
  });
  const auto metadata(
      GetNodeMetadata(reader, node->As<json::Object>(), Timestamp(), false));
  EXPECT_EQ(1, metadata.ids.size());
  EXPECT_EQ("k8s_node.testname", metadata.ids[0]);
  EXPECT_EQ(MonitoredResource("k8s_node", {
    {"cluster_name", "TestClusterName"},
    {"node_name", "testname"},
    {"location", "TestClusterLocation"},
  }), metadata.resource);
  EXPECT_EQ("TestVersion", metadata.metadata.version);
  EXPECT_EQ(false, metadata.metadata.is_deleted);
  EXPECT_EQ(time::rfc3339::FromString("2018-03-03T01:23:45.678901234Z"),
            metadata.metadata.created_at);
  EXPECT_EQ(Timestamp(), metadata.metadata.collected_at);
  json::value big = json::object({
    {"blobs", json::object({
      {"association", json::object({
        {"version", json::string("TestVersion")},
        {"raw", json::object({
          {"infrastructureResource",
           InstanceReader::InstanceResource(environment).ToJSON()},
        })},
      })},
      {"api", json::object({
        {"version", json::string("1.6")},  // Hard-coded in kubernetes.cc
        {"raw", std::move(node)},
      })},
    })},
  });
  EXPECT_EQ(big->ToString(), metadata.metadata.metadata->ToString());
}
}  // namespace google
