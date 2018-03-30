#include <boost/algorithm/string/join.hpp>

#include "../src/configuration.h"
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

  json::value ComputePodAssociations(const json::Object* pod,
				     const KubernetesReader& reader) {
    return reader.ComputePodAssociations(pod);
  }

  void InsertIntoOwners(const std::string& key, json::value& value,
			KubernetesReader* reader) {
    reader->owners_[key] = value->Clone();
  }
};

TEST_F(KubernetesTest, GetNodeMetadata) {
  Configuration config(std::stringstream(
    "KubernetesClusterName: TestClusterName\n"
    "KubernetesClusterLocation: TestClusterLocation\n"
    "MetadataIngestionRawContentVersion: TestVersion\n"
    "InstanceResourceType: gce_instance\n"
    "InstanceZone: TestZone\n"
    "InstanceId: TestID\n"
  ));
  Environment environment(config);
  KubernetesReader reader(config, nullptr);  // Don't need HealthChecker.
  json::value node = json::object({
    {"metadata", json::object({
      {"name", json::string("testname")},
      {"creationTimestamp", json::string("2018-03-03T01:23:45.678901234Z")},
    })}
  });
  const auto m =
      GetNodeMetadata(reader, node->As<json::Object>(), Timestamp(), false);
  EXPECT_EQ(1, m.ids.size());
  EXPECT_EQ("k8s_node.testname", m.ids[0]);
  EXPECT_EQ(MonitoredResource("k8s_node", {
    {"cluster_name", "TestClusterName"},
    {"node_name", "testname"},
    {"location", "TestClusterLocation"},
  }), m.resource);
  EXPECT_EQ("TestVersion", m.metadata.version);
  EXPECT_EQ(false, m.metadata.is_deleted);
  EXPECT_EQ(time::rfc3339::FromString("2018-03-03T01:23:45.678901234Z"),
            m.metadata.created_at);
  EXPECT_EQ(Timestamp(), m.metadata.collected_at);
  json::value big = json::object({
    {"blobs", json::object({
      {"association", json::object({
        {"version", json::string("TestVersion")},
        {"raw", json::object({
          {"infrastructureResource", json::object({
            {"type", json::string("gce_instance")},
            {"labels", json::object({
              {"instance_id", json::string("TestID")},
              {"zone", json::string("TestZone")},
            })},
          })},
        })},
      })},
      {"api", json::object({
        {"version", json::string("1.6")},  // Hard-coded in kubernetes.cc.
        {"raw", std::move(node)},
      })},
    })},
  });
  EXPECT_EQ(big->ToString(), m.metadata.metadata->ToString());
}

TEST_F(KubernetesTest, ComputePodAssociations) {
  const std::string api_version = "1.2.3";
  const std::string kind = "TestKind";
  const std::string uid1 = "TestUID1";
  Configuration config(std::stringstream(
    "KubernetesClusterName: TestClusterName\n"
    "KubernetesClusterLocation: TestClusterLocation\n"
    "MetadataIngestionRawContentVersion: TestVersion\n"
    "InstanceZone: TestZone\n"
    "InstanceId: TestID\n"
  ));
  Environment environment(config);
  KubernetesReader reader(config, nullptr);  // Don't need HealthChecker.
  const std::string encoded_ref = boost::algorithm::join(
      std::vector<std::string>{api_version, kind, uid1}, "/");
  json::value controller = json::object({
          {"controller", json::boolean(true)},
          {"apiVersion", json::string(api_version)},
          {"kind", json::string(kind)},
          {"name", json::string("TestName")},
          {"uid", json::string(uid1)},
	  {"metadata", json::object({
	    {"name", json::string("InnerTestName")},
	    {"kind", json::string("InnerTestKind")},
	    {"uid", json::string("InnerTestUID1")},
	  })},
  });
  InsertIntoOwners(encoded_ref, controller, &reader);
  json::value pod = json::object({
    {"metadata", json::object({
      {"namespace", json::string("TestNamespace")},
      {"uid", json::string("TestUID0")},
      {"ownerReferences", json::array({
        json::object({{"no_controller", json::boolean(true)}}),
	controller->Clone(),
      })},
    })},
    {"spec", json::object({
      {"nodeName", json::string("TestSpecNodeName")},
    })},
  });

  auto associations = ComputePodAssociations(pod->As<json::Object>(), reader);

  EXPECT_EQ(associations->ToString(), json::object({
    {"raw", json::object({
      {"controllers", json::object({
	{"topLevelControllerName", json::string("InnerTestName")},
	{"topLevelControllerType", json::string(kind)},
      })},
      {"infrastructureResource", json::object({
	{"labels", json::object({
	  {"instance_id", json::string("TestID")},
	  {"zone", json::string("TestZone")}
	})},
	{"type", json::string("gce_instance")},
      })},
      {"nodeName", json::string("TestSpecNodeName")},
    })},
    {"version", json::string("TestVersion")},
  })->ToString());
}
}  // namespace google
