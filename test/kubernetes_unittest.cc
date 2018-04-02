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

  MetadataUpdater::ResourceMetadata GetPodMetadata(
      const KubernetesReader& reader, const json::Object* pod,
      json::value associations, Timestamp collected_at, bool is_deleted) const
      throw(json::Exception) {
    return reader.GetPodMetadata(
        pod, std::move(associations), collected_at, is_deleted);
  }

  MetadataUpdater::ResourceMetadata GetLegacyResource(
      const KubernetesReader& reader, const json::Object* pod,
      const std::string& container_name) const throw(json::Exception) {
    return reader.GetLegacyResource(pod, container_name);
  }

  json::value ComputePodAssociations(const KubernetesReader& reader,
                                     const json::Object* pod) {
    return reader.ComputePodAssociations(pod);
  }

  void UpdateOwnersCache(KubernetesReader* reader, const std::string& key,
                         const json::value& value) {
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
  EXPECT_EQ(1, m.ids().size());
  EXPECT_EQ("k8s_node.testname", m.ids()[0]);
  EXPECT_EQ(MonitoredResource("k8s_node", {
    {"cluster_name", "TestClusterName"},
    {"node_name", "testname"},
    {"location", "TestClusterLocation"},
  }), m.resource());
  EXPECT_EQ("TestVersion", m.metadata().version);
  EXPECT_FALSE(m.metadata().is_deleted);
  EXPECT_EQ(time::rfc3339::FromString("2018-03-03T01:23:45.678901234Z"),
            m.metadata().created_at);
  EXPECT_EQ(Timestamp(), m.metadata().collected_at);
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
  EXPECT_EQ(big->ToString(), m.metadata().metadata->ToString());
}

TEST_F(KubernetesTest, ComputePodAssociations) {
  Configuration config(std::stringstream(
    "KubernetesClusterName: TestClusterName\n"
    "KubernetesClusterLocation: TestClusterLocation\n"
    "MetadataIngestionRawContentVersion: TestVersion\n"
    "InstanceZone: TestZone\n"
    "InstanceId: TestID\n"
  ));
  Environment environment(config);
  KubernetesReader reader(config, nullptr);  // Don't need HealthChecker.
  json::value controller = json::object({
    {"controller", json::boolean(true)},
    {"apiVersion", json::string("1.2.3")},
    {"kind", json::string("TestKind")},
    {"name", json::string("TestName")},
    {"uid", json::string("TestUID1")},
    {"metadata", json::object({
      {"name", json::string("InnerTestName")},
      {"kind", json::string("InnerTestKind")},
      {"uid", json::string("InnerTestUID1")},
    })},
  });
  UpdateOwnersCache(&reader, "1.2.3/TestKind/TestUID1", controller);
  json::value pod = json::object({
    {"metadata", json::object({
      {"namespace", json::string("TestNamespace")},
      {"uid", json::string("TestUID0")},
      {"ownerReferences", json::array({
        json::object({{"no_controller", json::boolean(true)}}),
        std::move(controller),
      })},
    })},
    {"spec", json::object({
      {"nodeName", json::string("TestSpecNodeName")},
    })},
  });

  json::value expected_associations = json::object({
    {"raw", json::object({
      {"controllers", json::object({
        {"topLevelControllerName", json::string("InnerTestName")},
        {"topLevelControllerType", json::string("TestKind")},
      })},
      {"infrastructureResource", json::object({
        {"labels", json::object({
          {"instance_id", json::string("TestID")},
          {"zone", json::string("TestZone")},
        })},
        {"type", json::string("gce_instance")},
      })},
      {"nodeName", json::string("TestSpecNodeName")},
    })},
    {"version", json::string("TestVersion")},
  });
  const auto associations =
      ComputePodAssociations(reader, pod->As<json::Object>());
  EXPECT_EQ(expected_associations->ToString(), associations->ToString());
}

TEST_F(KubernetesTest, GetPodMetadata) {
  Configuration config(std::stringstream(
    "KubernetesClusterName: TestClusterName\n"
    "KubernetesClusterLocation: TestClusterLocation\n"
    "MetadataApiResourceTypePerarator: \",\"\n"
    "MetadataIngestionRawContentVersion: TestVersion\n"
  ));
  Environment environment(config);
  KubernetesReader reader(config, nullptr);  // Don't need HealthChecker.

  json::value pod = json::object({
    {"metadata", json::object({
      {"namespace", json::string("TestNamespace")},
      {"name", json::string("TestName")},
      {"uid", json::string("TestUid")},
      {"creationTimestamp", json::string("2018-03-03T01:23:45.678901234Z")},
    })},
  });
  const auto m = GetPodMetadata(reader, pod->As<json::Object>(),
                                json::string("TestAssociations"), Timestamp(),
                                false);

  EXPECT_EQ(std::vector<std::string>(
      {"k8s_pod.TestUid", "k8s_pod.TestNamespace.TestName"}), m.ids());
  EXPECT_EQ(MonitoredResource("k8s_pod", {
    {"cluster_name", "TestClusterName"},
    {"pod_name", "TestName"},
    {"location", "TestClusterLocation"},
    {"namespace_name", "TestNamespace"},
  }), m.resource());
  EXPECT_EQ("TestVersion", m.metadata().version);
  EXPECT_FALSE(m.metadata().is_deleted);
  EXPECT_EQ(time::rfc3339::FromString("2018-03-03T01:23:45.678901234Z"),
            m.metadata().created_at);
  EXPECT_EQ(Timestamp(), m.metadata().collected_at);
  EXPECT_FALSE(m.metadata().ignore);
  json::value expected_metadata = json::object({
    {"blobs", json::object({
      {"api", json::object({
        {"raw", json::object({
          {"metadata", json::object({
            {"creationTimestamp",
              json::string("2018-03-03T01:23:45.678901234Z")},
            {"name", json::string("TestName")},
            {"namespace", json::string("TestNamespace")},
            {"uid", json::string("TestUid")},
          })},
        })},
        {"version", json::string("1.6")},
      })},
      {"association", json::string("TestAssociations")},
    })},
  });
  EXPECT_EQ(expected_metadata->ToString(), m.metadata().metadata->ToString());
}

TEST_F(KubernetesTest, GetLegacyResource) {
  Configuration config(std::stringstream(
    "KubernetesClusterName: TestClusterName\n"
    "MetadataApiResourceTypeSeparator: \".\"\n"
    "InstanceZone: TestZone\n"
    "InstanceId: TestID\n"
  ));
  Environment environment(config);
  KubernetesReader reader(config, nullptr);  // Don't need HealthChecker.
  json::value pod = json::object({
    {"metadata", json::object({
      {"namespace", json::string("TestNamespace")},
      {"name", json::string("TestName")},
      {"uid", json::string("TestUid")},
    })}
  });
  const auto m = GetLegacyResource(reader, json::object({
    {"metadata", json::object({
      {"namespace", json::string("TestNamespace")},
      {"name", json::string("TestName")},
      {"uid", json::string("TestUid")},
    })},
  })->As<json::Object>(), "TestContainerName");
  EXPECT_EQ(std::vector<std::string>({
    "gke_container.TestNamespace.TestUid.TestContainerName",
    "gke_container.TestNamespace.TestName.TestContainerName"
  }), m.ids());
  EXPECT_EQ(MonitoredResource("gke_container", {
    {"cluster_name", "TestClusterName"},
    {"container_name", "TestContainerName"},
    {"instance_id", "TestID"},
    {"namespace_id", "TestNamespace"},
    {"pod_id", "TestUid"},
    {"zone", "TestZone"},
  }), m.resource());
  EXPECT_EQ("", m.metadata().version);
  EXPECT_FALSE(m.metadata().is_deleted);
  EXPECT_EQ(Timestamp(), m.metadata().created_at);
  EXPECT_EQ(Timestamp(), m.metadata().collected_at);
  EXPECT_TRUE(m.metadata().ignore);
  EXPECT_EQ(json::object({})->ToString(), m.metadata().metadata->ToString());
}
}  // namespace google
