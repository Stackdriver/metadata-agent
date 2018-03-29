#include "../src/kubernetes.h"
#include "../src/updater.h"
#include "../src/configuration.h"
#include "../src/instance.h"
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
    "KubernetesClusterName: 'TestClusterName'\n"
    "KubernetesClusterLocation: 'TestClusterLocation'\n"
    "MetadataIngestionRawContentVersion: 'TestVersion'\n"
    "InstanceZone: 'TestZone'\n"
    "InstanceId: 'TestID'\n"
  ));
  Environment environment(config);
  KubernetesReader reader(config, nullptr);
  json::value node_json = json::object({
    {"metadata", json::object({
      {"name", json::string("testname")},
      {"creationTimestamp", json::string("2018-03-03T01:23:45.678901234Z")}
    })}
  });
  const json::Object* node = node_json->As<json::Object>();
  MetadataUpdater::ResourceMetadata metadata(
      GetNodeMetadata(reader, node, Timestamp(), false));
  EXPECT_EQ(metadata.ids.size(), 1);
  EXPECT_EQ(metadata.ids[0], "k8s_node.testname");
  EXPECT_EQ(metadata.resource,
            MonitoredResource("k8s_node", {
                                {"cluster_name", "TestClusterName"},
                                {"node_name", "testname"},
                                {"location", "TestClusterLocation"}
                              }));
  EXPECT_EQ(metadata.metadata.version, "TestVersion");
  EXPECT_EQ(metadata.metadata.is_deleted, false);
  EXPECT_EQ(metadata.metadata.created_at,
            time::rfc3339::FromString("2018-03-03T01:23:45.678901234Z"));
  EXPECT_EQ(metadata.metadata.collected_at, Timestamp());
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
        // 1.6 is hardcoded as kKubernetesApiVersion
        {"version", json::string("1.6")},
        {"raw", node->Clone()},
      })},
    })},
  });
  EXPECT_EQ(big->As<json::Object>()->ToString(),
            metadata.metadata.metadata->As<json::Object>()->ToString());
}
} // namespace google
