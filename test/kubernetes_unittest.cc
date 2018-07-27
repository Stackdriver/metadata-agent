#include "../src/configuration.h"
#include "../src/resource.h"
#include "../src/kubernetes.h"
#include "../src/updater.h"
#include "gtest/gtest.h"

namespace google {

class KubernetesTest : public ::testing::Test {
 protected:
  static MetadataUpdater::ResourceMetadata GetObjectMetadata(
      const KubernetesReader& reader, const json::Object *object,
      Timestamp collected_at, bool is_deleted)
      throw(json::Exception) {
    return reader.GetObjectMetadata(object, collected_at, is_deleted);
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

};

TEST_F(KubernetesTest, GetObjectMetadataService) {
  Configuration config(std::stringstream(
    "ProjectId: TestProjectId\n"
    "KubernetesClusterName: TestClusterName\n"
    "KubernetesClusterLocation: TestClusterLocation\n"
    "MetadataApiResourceTypeSeparator: \".\"\n"
  ));
  Environment environment(config);
  KubernetesReader reader(config, nullptr);  // Don't need HealthChecker.

  json::value service = json::object({
    {"apiVersion", json::string("ServiceVersion")},
    {"kind", json::string("Service")},
    {"metadata", json::object({
      {"namespace", json::string("TestNamespace")},
      {"name", json::string("TestName")},
      {"selfLink",
       json::string("/api/v1/namespaces/TestNamespace/services/TestName")},
      {"uid", json::string("TestUid")},
      {"creationTimestamp", json::string("2018-03-03T01:23:45.678901234Z")},
    })},
  });
  const auto m = GetObjectMetadata(reader, service->As<json::Object>(),
                                     Timestamp(), false);

  EXPECT_TRUE(m.ids().empty());
  EXPECT_EQ(MonitoredResource("", {}), m.resource());
  EXPECT_EQ("//container.googleapis.com/projects/TestProjectId/locations/"
            "TestClusterLocation/clusters/TestClusterName/k8s/namespaces/"
            "TestNamespace/services/TestName",
            m.metadata().name);
  EXPECT_EQ("ServiceVersion", m.metadata().version);
  EXPECT_EQ("io.k8s.Service", m.metadata().type);
  EXPECT_EQ("TestClusterLocation", m.metadata().location);
  EXPECT_EQ(
    "//container.googleapis.com/resourceTypes/io.k8s.Service/versions/"
    "ServiceVersion",
    m.metadata().schema_name);
  EXPECT_FALSE(m.metadata().is_deleted);
  EXPECT_EQ(Timestamp(), m.metadata().collected_at);
  EXPECT_FALSE(m.metadata().ignore);
  EXPECT_EQ(service->ToString(), m.metadata().metadata->ToString());
}

TEST_F(KubernetesTest, GetObjectMetadataEndpoints) {
  Configuration config(std::stringstream(
    "ProjectId: TestProjectId\n"
    "KubernetesClusterName: TestClusterName\n"
    "KubernetesClusterLocation: TestClusterLocation\n"
    "MetadataApiResourceTypeSeparator: \".\"\n"
  ));
  Environment environment(config);
  KubernetesReader reader(config, nullptr);  // Don't need HealthChecker.

  json::value service = json::object({
    {"apiVersion", json::string("EndpointsVersion")},
    {"kind", json::string("Endpoints")},
    {"metadata", json::object({
      {"namespace", json::string("TestNamespace")},
      {"name", json::string("TestName")},
      {"selfLink",
       json::string("/api/v1/namespaces/TestNamespace/endpoints/TestName")},
      {"uid", json::string("TestUid")},
      {"creationTimestamp", json::string("2018-03-03T01:23:45.678901234Z")},
    })},
  });
  const auto m = GetObjectMetadata(reader, service->As<json::Object>(),
                                     Timestamp(), false);

  EXPECT_TRUE(m.ids().empty());
  EXPECT_EQ(MonitoredResource("", {}), m.resource());
  EXPECT_EQ("//container.googleapis.com/projects/TestProjectId/locations/"
            "TestClusterLocation/clusters/TestClusterName/k8s/namespaces/"
            "TestNamespace/endpoints/TestName",
            m.metadata().name);
  EXPECT_EQ("EndpointsVersion", m.metadata().version);
  EXPECT_EQ("io.k8s.Endpoints", m.metadata().type);
  EXPECT_EQ("TestClusterLocation", m.metadata().location);
  EXPECT_EQ(
    "//container.googleapis.com/resourceTypes/io.k8s.Endpoints/versions/"
    "EndpointsVersion",
    m.metadata().schema_name);
  EXPECT_FALSE(m.metadata().is_deleted);
  EXPECT_EQ(Timestamp(), m.metadata().collected_at);
  EXPECT_FALSE(m.metadata().ignore);
  EXPECT_EQ(service->ToString(), m.metadata().metadata->ToString());
}

TEST_F(KubernetesTest, GetNodeMetadata) {
  Configuration config(std::istringstream(
    "ProjectId: TestProjectId\n"
    "KubernetesClusterName: TestClusterName\n"
    "KubernetesClusterLocation: TestClusterLocation\n"
  ));
  Environment environment(config);
  KubernetesReader reader(config, nullptr);  // Don't need HealthChecker.
  json::value node = json::object({
    {"apiVersion", json::string("NodeVersion")},
    {"kind", json::string("Node")},
    {"metadata", json::object({
      {"name", json::string("testname")},
      {"selfLink", json::string("/api/v1/nodes/testname")},
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
  EXPECT_EQ("//container.googleapis.com/projects/TestProjectId/locations/"
            "TestClusterLocation/clusters/TestClusterName/k8s/nodes/testname",
            m.metadata().name);
  EXPECT_EQ("NodeVersion", m.metadata().version);
  EXPECT_EQ("io.k8s.Node", m.metadata().type);
  EXPECT_EQ("TestClusterLocation", m.metadata().location);
  EXPECT_EQ(
    "//container.googleapis.com/resourceTypes/io.k8s.Node/versions/NodeVersion",
    m.metadata().schema_name);
  EXPECT_FALSE(m.metadata().is_deleted);
  EXPECT_EQ(Timestamp(), m.metadata().collected_at);
  EXPECT_EQ(node->ToString(), m.metadata().metadata->ToString());
}

TEST_F(KubernetesTest, GetPodMetadata) {
  Configuration config(std::stringstream(
    "ProjectId: TestProjectId\n"
    "KubernetesClusterName: TestClusterName\n"
    "KubernetesClusterLocation: TestClusterLocation\n"
    "MetadataApiResourceTypeSeparator: \".\"\n"
  ));
  Environment environment(config);
  KubernetesReader reader(config, nullptr);  // Don't need HealthChecker.

  json::value pod = json::object({
    {"apiVersion", json::string("PodVersion")},
    {"kind", json::string("Pod")},
    {"metadata", json::object({
      {"namespace", json::string("TestNamespace")},
      {"name", json::string("TestName")},
      {"selfLink",
       json::string("/api/v1/namespaces/TestNamespace/pods/TestName")},
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
  EXPECT_EQ("//container.googleapis.com/projects/TestProjectId/locations/"
            "TestClusterLocation/clusters/TestClusterName/k8s/namespaces/"
            "TestNamespace/pods/TestName",
            m.metadata().name);
  EXPECT_EQ("PodVersion", m.metadata().version);
  EXPECT_EQ("io.k8s.Pod", m.metadata().type);
  EXPECT_EQ("TestClusterLocation", m.metadata().location);
  EXPECT_EQ(
    "//container.googleapis.com/resourceTypes/io.k8s.Pod/versions/PodVersion",
    m.metadata().schema_name);
  EXPECT_FALSE(m.metadata().is_deleted);
  EXPECT_EQ(Timestamp(), m.metadata().collected_at);
  EXPECT_FALSE(m.metadata().ignore);
  EXPECT_EQ(pod->ToString(), m.metadata().metadata->ToString());
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
    "ProjectId: TestProjectId\n"
    "KubernetesClusterName: TestClusterName\n"
    "KubernetesClusterLocation: TestClusterLocation\n"
    "MetadataApiResourceTypeSeparator: \".\"\n"
    "InstanceZone: TestZone\n"
    "InstanceId: TestID\n"
  ));
  Environment environment(config);
  KubernetesReader reader(config, nullptr);  // Don't need HealthChecker.

  json::value pod = json::object({
    {"apiVersion", json::string("PodVersion")},
    {"kind", json::string("Pod")},
    {"metadata", json::object({
      {"name", json::string("TestPodName")},
      {"selfLink",
       json::string("/api/v1/namespaces/TestNamespace/pods/TestPodName")},
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
  EXPECT_EQ("//container.googleapis.com/projects/TestProjectId/locations/"
            "TestClusterLocation/clusters/TestClusterName/k8s/namespaces/"
            "TestNamespace/pods/TestPodName",
            m[2].metadata().name);
  EXPECT_EQ("PodVersion", m[2].metadata().version);
  EXPECT_FALSE(m[2].metadata().is_deleted);
  EXPECT_EQ(Timestamp(), m[2].metadata().collected_at);
  EXPECT_EQ(pod->ToString(), m[2].metadata().metadata->ToString());
}
}  // namespace google
