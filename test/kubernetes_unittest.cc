#include "../src/configuration.h"
#include "../src/resource.h"
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

  json::value ComputePodAssociations(const KubernetesReader& reader,
                                     const json::Object* pod) {
    return reader.ComputePodAssociations(pod);
  }

  void UpdateOwnersCache(KubernetesReader* reader, const std::string& key,
                         const json::value& value) {
    reader->owners_[key] = value->Clone();
  MetadataUpdater::ResourceMetadata GetClusterMetadata(
      const KubernetesReader& reader, Timestamp collected_at) const
      throw(json::Exception) {
    return reader.GetClusterMetadata(collected_at);
  }

  void UpdateServiceToMetadataCache(
      KubernetesReader* reader, const json::Object* service, bool is_deleted)
      throw(json::Exception) {
    return reader->UpdateServiceToMetadataCache(service, is_deleted);
  }

  void UpdateServiceToPodsCache(
      KubernetesReader* reader, const json::Object* endpoints, bool is_deleted)
      throw(json::Exception) {
    return reader->UpdateServiceToPodsCache(endpoints, is_deleted);
  }
};

TEST_F(KubernetesTest, GetNodeMetadata) {
  Configuration config(std::istringstream(
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

TEST_F(KubernetesTest, GetClusterMetadataEmpty) {
  Configuration config(std::istringstream(
    "KubernetesClusterName: TestClusterName\n"
    "KubernetesClusterLocation: TestClusterLocation\n"
    "MetadataIngestionRawContentVersion: TestVersion\n"
  ));
  Environment environment(config);
  KubernetesReader reader(config, nullptr);  // Don't need HealthChecker.
  const auto m = GetClusterMetadata(reader, Timestamp());
  EXPECT_TRUE(m.ids().empty());
  EXPECT_EQ(MonitoredResource("k8s_cluster", {
    {"cluster_name", "TestClusterName"},
    {"location", "TestClusterLocation"},
  }), m.resource());
  EXPECT_EQ("TestVersion", m.metadata().version);
  EXPECT_FALSE(m.metadata().is_deleted);
  EXPECT_EQ(Timestamp(), m.metadata().created_at);
  EXPECT_EQ(Timestamp(), m.metadata().collected_at);
  json::value empty_cluster = json::object({
    {"blobs", json::object({
      {"services", json::array({})},
    })},
  });
  EXPECT_EQ(empty_cluster->ToString(), m.metadata().metadata->ToString());
}

TEST_F(KubernetesTest, GetClusterMetadataEmptyService) {
  Configuration config(std::istringstream(
    "KubernetesClusterName: TestClusterName\n"
    "KubernetesClusterLocation: TestClusterLocation\n"
    "MetadataIngestionRawContentVersion: TestVersion\n"
  ));
  Environment environment(config);
  json::value service = json::object({
    {"metadata", json::object({
      {"name", json::string("testname")},
      {"namespace", json::string("testnamespace")},
    })},
  });
  KubernetesReader reader(config, nullptr);  // Don't need HealthChecker.
  UpdateServiceToMetadataCache(
      &reader, service->As<json::Object>(), /*is_deleted=*/false);
  const auto m = GetClusterMetadata(reader, Timestamp());
  EXPECT_EQ(0, m.ids().size());
  EXPECT_EQ(MonitoredResource("k8s_cluster", {
    {"cluster_name", "TestClusterName"},
    {"location", "TestClusterLocation"},
  }), m.resource());
  EXPECT_EQ("TestVersion", m.metadata().version);
  EXPECT_FALSE(m.metadata().is_deleted);
  EXPECT_EQ(Timestamp(), m.metadata().created_at);
  EXPECT_EQ(Timestamp(), m.metadata().collected_at);
  json::value expected_cluster = json::object({
    {"blobs", json::object({
      {"services", json::array({
        json::object({
          {"api", json::object({
            {"pods", json::array({})},
            {"raw", std::move(service)},
            {"version", json::string("1.6")},  // Hard-coded in kubernetes.cc.
          })},
        }),
      })},
    })},
  });
  EXPECT_EQ(expected_cluster->ToString(), m.metadata().metadata->ToString());
}

TEST_F(KubernetesTest, GetClusterMetadataServiceWithPods) {
  Configuration config(std::istringstream(
    "KubernetesClusterName: TestClusterName\n"
    "KubernetesClusterLocation: TestClusterLocation\n"
    "MetadataIngestionRawContentVersion: TestVersion\n"
  ));
  Environment environment(config);
  json::value service = json::object({
    {"metadata", json::object({
      {"name", json::string("testname")},
      {"namespace", json::string("testnamespace")},
    })},
  });
  json::value endpoints = json::object({
    {"metadata", json::object({
      {"name", json::string("testname")},
      {"namespace", json::string("testnamespace")},
    })},
    {"subsets", json::array({
      json::object({
        {"addresses", json::array({
          json::object({
            {"targetRef", json::object({
              {"kind", json::string("Pod")},
              {"name", json::string("my-pod")},
            })},
          }),
        })},
      }),
    })},
  });
  KubernetesReader reader(config, nullptr);  // Don't need HealthChecker.
  UpdateServiceToMetadataCache(
      &reader, service->As<json::Object>(), /*is_deleted=*/false);
  UpdateServiceToPodsCache(
      &reader, endpoints->As<json::Object>(), /*is_deleted=*/false);
  const auto m = GetClusterMetadata(reader, Timestamp());
  EXPECT_TRUE(m.ids().empty());
  EXPECT_EQ(MonitoredResource("k8s_cluster", {
    {"cluster_name", "TestClusterName"},
    {"location", "TestClusterLocation"},
  }), m.resource());
  EXPECT_EQ("TestVersion", m.metadata().version);
  EXPECT_FALSE(m.metadata().is_deleted);
  EXPECT_EQ(Timestamp(), m.metadata().created_at);
  EXPECT_EQ(Timestamp(), m.metadata().collected_at);
  MonitoredResource pod_mr = MonitoredResource("k8s_pod", {
    {"cluster_name", "TestClusterName"},
    {"namespace_name", "testnamespace"},
    {"pod_name", "my-pod"},
    {"location", "TestClusterLocation"},
  });
  json::value expected_cluster = json::object({
    {"blobs", json::object({
      {"services", json::array({
        json::object({
          {"api", json::object({
            {"pods", json::array({
              pod_mr.ToJSON(),
            })},
            {"raw", std::move(service)},
            {"version", json::string("1.6")},  // Hard-coded in kubernetes.cc.
          })},
        }),
      })},
    })},
  });
  EXPECT_EQ(expected_cluster->ToString(), m.metadata().metadata->ToString());
}

TEST_F(KubernetesTest, GetClusterMetadataDeletedService) {
  Configuration config(std::istringstream(
    "KubernetesClusterName: TestClusterName\n"
    "KubernetesClusterLocation: TestClusterLocation\n"
    "MetadataIngestionRawContentVersion: TestVersion\n"
  ));
  Environment environment(config);
  json::value service = json::object({
    {"metadata", json::object({
      {"name", json::string("testname")},
      {"namespace", json::string("testnamespace")},
    })},
  });
  KubernetesReader reader(config, nullptr);  // Don't need HealthChecker.
  UpdateServiceToMetadataCache(
      &reader, service->As<json::Object>(), /*is_deleted=*/false);
  UpdateServiceToMetadataCache(
      &reader, service->As<json::Object>(), /*is_deleted=*/true);
  const auto m = GetClusterMetadata(reader, Timestamp());
  EXPECT_TRUE(m.ids().empty());
  json::value empty_cluster = json::object({
    {"blobs", json::object({
      {"services", json::array({})},
    })},
  });
  EXPECT_EQ(empty_cluster->ToString(), m.metadata().metadata->ToString());
}
}  // namespace google
