#include "../src/configuration.h"
#include "../src/resource.h"
#include "../src/kubernetes.h"
#include "../src/updater.h"
#include "fake_http_server.h"
#include "gtest/gtest.h"
#include "temp_file.h"

namespace google {

class KubernetesTest : public ::testing::Test {
 protected:
  using QueryException = KubernetesReader::QueryException;

  static json::value QueryMaster(
      const KubernetesReader& reader, const std::string& path)
      throw(QueryException, json::Exception) {
    return reader.QueryMaster(path);
  }

  static MetadataUpdater::ResourceMetadata GetNodeMetadata(
      const KubernetesReader& reader, const json::Object *node,
      Timestamp collected_at, bool is_deleted)
      throw(json::Exception) {
    return reader.GetNodeMetadata(node, collected_at, is_deleted);
  }

  static MetadataUpdater::ResourceMetadata GetPodMetadata(
      const KubernetesReader& reader, const json::Object* pod,
      Timestamp collected_at, bool is_deleted) throw(json::Exception) {
    return reader.GetPodMetadata(pod, collected_at, is_deleted);
  }

  static MetadataUpdater::ResourceMetadata GetContainerMetadata(
      const KubernetesReader& reader, const json::Object* pod,
      const json::Object* container_spec, const json::Object* container_status,
      Timestamp collected_at, bool is_deleted) throw(json::Exception) {
    return reader.GetContainerMetadata(pod, container_spec, container_status,
                                       collected_at, is_deleted);
  }

  static std::vector<MetadataUpdater::ResourceMetadata> GetPodAndContainerMetadata(
      const KubernetesReader& reader, const json::Object* pod,
      Timestamp collected_at, bool is_deleted) throw(json::Exception) {
    return reader.GetPodAndContainerMetadata(pod, collected_at, is_deleted);
  }

  static MetadataUpdater::ResourceMetadata GetLegacyResource(
      const KubernetesReader& reader, const json::Object* pod,
      const std::string& container_name) throw(json::Exception) {
    return reader.GetLegacyResource(pod, container_name);
  }

  static const std::string& KubernetesApiToken(const KubernetesReader& reader) {
    return reader.KubernetesApiToken();
  }

  static const std::string& KubernetesNamespace(const KubernetesReader& reader) {
    return reader.KubernetesNamespace();
  }

  static void SetServiceAccountDirectoryForTest(
      KubernetesReader* reader, const std::string& directory) {
    reader->SetServiceAccountDirectoryForTest(directory);
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
  json::value expected_metadata = json::object({
    {"blobs", json::object({
      {"api", json::object({
        {"version", json::string("1.6")},  // Hard-coded in kubernetes.cc.
        {"raw", std::move(node)},
      })},
    })},
  });
  EXPECT_EQ(expected_metadata->ToString(), m.metadata().metadata->ToString());
}

TEST_F(KubernetesTest, GetPodMetadata) {
  Configuration config(std::stringstream(
    "KubernetesClusterName: TestClusterName\n"
    "KubernetesClusterLocation: TestClusterLocation\n"
    "MetadataApiResourceTypeSeparator: \".\"\n"
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
                                Timestamp(), false);

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
    })},
  });
  const auto m = GetLegacyResource(reader, pod->As<json::Object>(),
                                   "TestContainerName");
  EXPECT_EQ(std::vector<std::string>({
    "gke_container.TestNamespace.TestUid.TestContainerName",
    "gke_container.TestNamespace.TestName.TestContainerName",
  }), m.ids());
  EXPECT_EQ(MonitoredResource("gke_container", {
    {"cluster_name", "TestClusterName"},
    {"container_name", "TestContainerName"},
    {"instance_id", "TestID"},
    {"namespace_id", "TestNamespace"},
    {"pod_id", "TestUid"},
    {"zone", "TestZone"},
  }), m.resource());
  EXPECT_TRUE(m.metadata().ignore);
}

TEST_F(KubernetesTest, GetContainerMetadata) {
  Configuration config(std::stringstream(
    "KubernetesClusterName: TestClusterName\n"
    "KubernetesClusterLocation: TestClusterLocation\n"
    "MetadataApiResourceTypeSeparator: \".\"\n"
  ));
  Environment environment(config);
  KubernetesReader reader(config, nullptr);  // Don't need HealthChecker.
  json::value pod = json::object({
    {"metadata", json::object({
      {"namespace", json::string("TestNamespace")},
      {"name", json::string("TestName")},
      {"uid", json::string("TestUid")},
      {"creationTimestamp", json::string("2018-03-03T01:23:45.678901234Z")},
      {"labels", json::object({{"label", json::string("TestLabel")}})},
    })},
  });
  json::value spec = json::object({{"name", json::string("TestSpecName")}});
  json::value status = json::object({
    {"containerID", json::string("docker://TestContainerID")},
  });
  const auto m = GetContainerMetadata(
      reader,
      pod->As<json::Object>(),
      spec->As<json::Object>(),
      status->As<json::Object>(),
      Timestamp(),
      /*is_deleted=*/false);

  EXPECT_EQ(std::vector<std::string>({
    "k8s_container.TestUid.TestSpecName",
    "k8s_container.TestNamespace.TestName.TestSpecName",
    "k8s_container.TestContainerID",
  }), m.ids());
  EXPECT_EQ(MonitoredResource("k8s_container", {
    {"cluster_name", "TestClusterName"},
    {"container_name", "TestSpecName"},
    {"location", "TestClusterLocation"},
    {"namespace_name", "TestNamespace"},
    {"pod_name", "TestName"},
  }), m.resource());
  EXPECT_TRUE(m.metadata().ignore);
}

TEST_F(KubernetesTest, GetPodAndContainerMetadata) {
  Configuration config(std::stringstream(
    "KubernetesClusterName: TestClusterName\n"
    "KubernetesClusterLocation: TestClusterLocation\n"
    "MetadataApiResourceTypeSeparator: \".\"\n"
    "MetadataIngestionRawContentVersion: TestVersion\n"
    "InstanceZone: TestZone\n"
    "InstanceId: TestID\n"
  ));
  Environment environment(config);
  KubernetesReader reader(config, nullptr);  // Don't need HealthChecker.

  json::value pod = json::object({
    {"metadata", json::object({
      {"name", json::string("TestPodName")},
      {"namespace", json::string("TestNamespace")},
      {"uid", json::string("TestPodUid")},
      {"creationTimestamp", json::string("2018-03-03T01:23:45.678901234Z")},
    })},
    {"spec", json::object({
      {"nodeName", json::string("TestSpecNodeName")},
      {"containers", json::array({
        json::object({{"name", json::string("TestContainerName0")}}),
      })},
    })},
    {"status", json::object({
      {"containerID", json::string("docker://TestContainerID")},
      {"containerStatuses", json::array({
        json::object({
          {"name", json::string("TestContainerName0")},
        }),
      })},
    })},
  });

  auto m = GetPodAndContainerMetadata(
      reader, pod->As<json::Object>(), Timestamp(), false);
  EXPECT_EQ(3, m.size());
  EXPECT_EQ(std::vector<std::string>({
    "gke_container.TestNamespace.TestPodUid.TestContainerName0",
    "gke_container.TestNamespace.TestPodName.TestContainerName0",
  }), m[0].ids());
  EXPECT_EQ(MonitoredResource("gke_container", {
    {"cluster_name", "TestClusterName"},
    {"container_name", "TestContainerName0"},
    {"instance_id", "TestID"},
    {"namespace_id", "TestNamespace"},
    {"pod_id", "TestPodUid"},
    {"zone", "TestZone"}
  }), m[0].resource());
  EXPECT_TRUE(m[0].metadata().ignore);

  EXPECT_EQ(std::vector<std::string>({
    "k8s_container.TestPodUid.TestContainerName0",
    "k8s_container.TestNamespace.TestPodName.TestContainerName0"
  }), m[1].ids());
  EXPECT_EQ(MonitoredResource("k8s_container", {
    {"cluster_name", "TestClusterName"},
    {"container_name", "TestContainerName0"},
    {"location", "TestClusterLocation"},
    {"namespace_name", "TestNamespace"},
    {"pod_name", "TestPodName"},
  }), m[1].resource());
  EXPECT_TRUE(m[1].metadata().ignore);

  EXPECT_EQ(std::vector<std::string>({
    "k8s_pod.TestPodUid",
    "k8s_pod.TestNamespace.TestPodName"
  }), m[2].ids());
  EXPECT_EQ(MonitoredResource("k8s_pod", {
      {"cluster_name", "TestClusterName"},
      {"location", "TestClusterLocation"},
      {"namespace_name", "TestNamespace"},
      {"pod_name", "TestPodName"},
  }), m[2].resource());
  EXPECT_FALSE(m[2].metadata().ignore);
  EXPECT_EQ("TestVersion", m[2].metadata().version);
  EXPECT_FALSE(m[2].metadata().is_deleted);
  EXPECT_EQ(time::rfc3339::FromString("2018-03-03T01:23:45.678901234Z"),
            m[2].metadata().created_at);
  EXPECT_EQ(Timestamp(), m[2].metadata().collected_at);
  json::value pod_metadata = json::object({
    {"blobs", json::object({
      {"api", json::object({
        {"raw", json::object({
          {"metadata", json::object({
            {"creationTimestamp",
              json::string("2018-03-03T01:23:45.678901234Z")},
            {"name", json::string("TestPodName")},
            {"namespace", json::string("TestNamespace")},
            {"uid", json::string("TestPodUid")},
          })},
          {"spec", json::object({
            {"containers", json::array({
              json::object({{"name", json::string("TestContainerName0")}})
            })},
            {"nodeName", json::string("TestSpecNodeName")},
          })},
          {"status", json::object({
            {"containerID", json::string("docker://TestContainerID")},
            {"containerStatuses", json::array({
              json::object({{"name", json::string("TestContainerName0")}})
            })},
          })},
        })},
        {"version", json::string("1.6")},
      })},
    })},
  });
  EXPECT_EQ(pod_metadata->ToString(),
            m[2].metadata().metadata->ToString());
}

TEST_F(KubernetesTest, KubernetesApiToken) {
  testing::TemporaryFile token_file("token", "the-api-token");

  Configuration config;
  Environment environment(config);
  KubernetesReader reader(config, nullptr);  // Don't need HealthChecker.
  SetServiceAccountDirectoryForTest(
      &reader, token_file.FullPath().parent_path().native());

  EXPECT_EQ("the-api-token", KubernetesApiToken(reader));

  // Check that the value is cached.
  token_file.SetContents("updated-api-token");
  EXPECT_EQ("the-api-token", KubernetesApiToken(reader));
}

TEST_F(KubernetesTest, KubernetesNamespace) {
  testing::TemporaryFile namespace_file("namespace", "the-namespace");

  Configuration config;
  Environment environment(config);
  KubernetesReader reader(config, nullptr);  // Don't need HealthChecker.
  SetServiceAccountDirectoryForTest(
      &reader, namespace_file.FullPath().parent_path().native());

  EXPECT_EQ("the-namespace", KubernetesNamespace(reader));

  // Check that the value is cached.
  namespace_file.SetContents("updated-namespace");
  EXPECT_EQ("the-namespace", KubernetesNamespace(reader));
}

TEST_F(KubernetesTest, QueryMaster) {
  testing::FakeServer server;
  server.SetResponse("/a/b/c", "{\"hello\":\"world\"}");

  Configuration config(std::istringstream(
    "KubernetesEndpointHost: " + server.GetUrl() + "\n"
  ));
  Environment environment(config);
  KubernetesReader reader(config, nullptr);  // Don't need HealthChecker.
  EXPECT_EQ(QueryMaster(reader, "/a/b/c")->ToString(), "{\"hello\":\"world\"}");

  EXPECT_THROW(QueryMaster(reader, "/d/e/f"), QueryException);
}

TEST_F(KubernetesTest, MetadataQuery) {
  json::value node = json::object({
    {"metadata", json::object({
      {"name", json::string("TestNodeName")},
      {"creationTimestamp", json::string("2018-03-03T01:23:45.678901234Z")},
    })}
  });
  json::value pod = json::object({
    {"apiVersion", json::string("1.2.3")},
    {"items", json::array({
      json::object({
        {"metadata", json::object({
          {"name", json::string("TestPodName")},
          {"namespace", json::string("TestNamespace")},
          {"uid", json::string("TestPodUid")},
          {"creationTimestamp", json::string("2018-03-03T01:23:45.678901234Z")},
        })},
        {"spec", json::object({
          {"nodeName", json::string("TestNodeName")},
          {"containers", json::array({
            json::object({{"name", json::string("TestContainerName0")}}),
          })},
        })},
        {"status", json::object({
          {"containerID", json::string("docker://TestContainerID")},
          {"containerStatuses", json::array({
            json::object({
              {"name", json::string("TestContainerName0")},
            }),
          })},
        })},
      }),
    })},
  });
  testing::FakeServer server;
  server.SetResponse("/api/v1/nodes/TestNodeName", node->ToString());
  server.SetResponse("/api/v1/pods?fieldSelector=spec.nodeName%3DTestNodeName",
                     pod->ToString());

  Configuration config(std::istringstream(
    "InstanceId: TestID\n"
    "InstanceZone: TestZone\n"
    "KubernetesClusterLocation: TestClusterLocation\n"
    "KubernetesClusterName: TestClusterName\n"
    "KubernetesEndpointHost: " + server.GetUrl() + "\n"
    "KubernetesNodeName: TestNodeName\n"
    "MetadataIngestionRawContentVersion: TestVersion\n"
  ));
  Environment environment(config);
  KubernetesReader reader(config, nullptr);  // Don't need HealthChecker.

  std::vector<KubernetesUpdater::ResourceMetadata> m = reader.MetadataQuery();
  EXPECT_EQ(4, m.size());

  // Verify node metadata.
  EXPECT_EQ(1, m[0].ids().size());
  EXPECT_EQ("k8s_node.TestNodeName", m[0].ids()[0]);
  EXPECT_EQ(MonitoredResource("k8s_node", {
    {"cluster_name", "TestClusterName"},
    {"node_name", "TestNodeName"},
    {"location", "TestClusterLocation"},
  }), m[0].resource());
  EXPECT_EQ("TestVersion", m[0].metadata().version);
  EXPECT_FALSE(m[0].metadata().is_deleted);
  EXPECT_EQ(time::rfc3339::FromString("2018-03-03T01:23:45.678901234Z"),
            m[0].metadata().created_at);
  json::value node_metadata = json::object({
    {"blobs", json::object({
      {"api", json::object({
        {"version", json::string("1.6")},  // Hard-coded in kubernetes.cc.
        {"raw", json::object({
          {"metadata", json::object({
            {"name", json::string("TestNodeName")},
            {"creationTimestamp", json::string("2018-03-03T01:23:45.678901234Z")},
          })},
        })},
      })},
    })},
  });
  EXPECT_EQ(node_metadata->ToString(), m[0].metadata().metadata->ToString());

  // Verify pod metadata.
  EXPECT_EQ(std::vector<std::string>({
    "gke_container.TestNamespace.TestPodUid.TestContainerName0",
    "gke_container.TestNamespace.TestPodName.TestContainerName0",
  }), m[1].ids());
  EXPECT_EQ(MonitoredResource("gke_container", {
    {"cluster_name", "TestClusterName"},
    {"container_name", "TestContainerName0"},
    {"instance_id", "TestID"},
    {"namespace_id", "TestNamespace"},
    {"pod_id", "TestPodUid"},
    {"zone", "TestZone"}
  }), m[1].resource());
  EXPECT_TRUE(m[1].metadata().ignore);

  EXPECT_EQ(std::vector<std::string>({
    "k8s_container.TestPodUid.TestContainerName0",
    "k8s_container.TestNamespace.TestPodName.TestContainerName0"
  }), m[2].ids());
  EXPECT_EQ(MonitoredResource("k8s_container", {
    {"cluster_name", "TestClusterName"},
    {"container_name", "TestContainerName0"},
    {"location", "TestClusterLocation"},
    {"namespace_name", "TestNamespace"},
    {"pod_name", "TestPodName"},
  }), m[2].resource());
  EXPECT_TRUE(m[2].metadata().ignore);

  EXPECT_EQ(std::vector<std::string>({
    "k8s_pod.TestPodUid",
    "k8s_pod.TestNamespace.TestPodName"
  }), m[3].ids());
  EXPECT_EQ(MonitoredResource("k8s_pod", {
      {"cluster_name", "TestClusterName"},
      {"location", "TestClusterLocation"},
      {"namespace_name", "TestNamespace"},
      {"pod_name", "TestPodName"},
  }), m[3].resource());
  EXPECT_FALSE(m[3].metadata().ignore);
  EXPECT_EQ("TestVersion", m[3].metadata().version);
  EXPECT_FALSE(m[3].metadata().is_deleted);
  EXPECT_EQ(time::rfc3339::FromString("2018-03-03T01:23:45.678901234Z"),
            m[3].metadata().created_at);
  json::value pod_metadata = json::object({
    {"blobs", json::object({
      {"api", json::object({
        {"raw", json::object({
          {"metadata", json::object({
            {"creationTimestamp",
              json::string("2018-03-03T01:23:45.678901234Z")},
            {"name", json::string("TestPodName")},
            {"namespace", json::string("TestNamespace")},
            {"uid", json::string("TestPodUid")},
          })},
          {"spec", json::object({
            {"containers", json::array({
              json::object({{"name", json::string("TestContainerName0")}})
            })},
            {"nodeName", json::string("TestNodeName")},
          })},
          {"status", json::object({
            {"containerID", json::string("docker://TestContainerID")},
            {"containerStatuses", json::array({
              json::object({{"name", json::string("TestContainerName0")}})
            })},
          })},
        })},
        {"version", json::string("1.6")},
      })},
    })},
  });
  EXPECT_EQ(pod_metadata->ToString(), m[3].metadata().metadata->ToString());
}

}  // namespace google
