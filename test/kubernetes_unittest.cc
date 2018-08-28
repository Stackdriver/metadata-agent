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
#include "../src/logging.h"
#include "../src/kubernetes.h"
#include "../src/updater.h"
#include "environment_util.h"
#include "fake_clock.h"
#include "fake_http_server.h"
#include "gtest/gtest.h"
#include "temp_file.h"

#include <memory>

namespace google {

class KubernetesTest : public ::testing::Test {
 public:
  virtual ~KubernetesTest() = default;

 protected:
  using QueryException = KubernetesReader::QueryException;

  static json::value QueryMaster(
      const KubernetesReader& reader, const std::string& path)
      throw(QueryException, json::Exception) {
    return reader.QueryMaster(path);
  }

  static const std::string ClusterFullName(const KubernetesReader& reader) {
    return reader.ClusterFullName();
  }

  static const std::string FullResourceName(
      const KubernetesReader& reader, const std::string& self_link) {
    return reader.FullResourceName(self_link);
  }

  static const std::string GetWatchPath(
      const KubernetesReader& reader, const std::string& plural_kind,
      const std::string& api_version, const std::string& selector) {
    return reader.GetWatchPath(plural_kind, api_version, selector);
  }

  static MetadataUpdater::ResourceMetadata GetObjectMetadata(
      const KubernetesReader& reader, const json::Object *object,
      Timestamp collected_at, bool is_deleted)
      throw(json::Exception) {
    return reader.GetObjectMetadata(
        object, collected_at, is_deleted,
        [](const json::Object* object) {
          return KubernetesReader::IdsAndMR{
            std::vector<std::string>{}, MonitoredResource("", {})};
        }
    );
  }

  static MetadataUpdater::ResourceMetadata GetNodeMetadata(
      const KubernetesReader& reader, const json::Object *node,
      Timestamp collected_at, bool is_deleted)
      throw(json::Exception) {
    auto cb = [&](const json::Object* node) {
      return reader.NodeResourceMappingCallback(node);
    };
    return reader.GetObjectMetadata(node, collected_at, is_deleted, cb);
  }

  static MetadataUpdater::ResourceMetadata GetPodMetadata(
      const KubernetesReader& reader, const json::Object* pod,
      Timestamp collected_at, bool is_deleted) throw(json::Exception) {
    auto cb = [&](const json::Object* pod) {
      return reader.PodResourceMappingCallback(pod);
    };
    return reader.GetObjectMetadata(pod, collected_at, is_deleted, cb);
  }

  static MetadataUpdater::ResourceMetadata GetContainerMetadata(
      const KubernetesReader& reader, const json::Object* pod,
      const json::Object* container_spec, const json::Object* container_status,
      Timestamp collected_at, bool is_deleted) throw(json::Exception) {
    return reader.GetContainerMetadata(pod, container_spec, container_status,
                                       collected_at, is_deleted);
  }

  static std::vector<MetadataUpdater::ResourceMetadata>
  GetPodAndContainerMetadata(
      const KubernetesReader& reader, const json::Object* pod,
      Timestamp collected_at, bool is_deleted) throw(json::Exception) {
    return reader.GetPodAndContainerMetadata(pod, collected_at, is_deleted);
  }

  static MetadataUpdater::ResourceMetadata GetLegacyResource(
      const KubernetesReader& reader, const json::Object* pod,
      const std::string& container_name)
      throw(std::out_of_range, json::Exception) {
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

  static void SetMetadataServerUrlForTest(
      KubernetesReader* reader, const std::string& url) {
    testing::EnvironmentUtil::SetMetadataServerUrlForTest(
        &reader->environment_, url);
  }

  void SetUp() override {
    config = CreateConfig();
    reader.reset(new KubernetesReader(*config,
                                      nullptr));  // Don't need HealthChecker.
  }

  virtual std::unique_ptr<Configuration> CreateConfig() = 0;

  std::unique_ptr<Configuration> config;
  std::unique_ptr<KubernetesReader> reader;
};

class KubernetesTestNoInstance : public KubernetesTest {
 protected:
  void SetUp() override {
    metadata_server.reset(new testing::FakeServer());
    KubernetesTest::SetUp();
    // Need to ensure that the metadata server returns no info either.
    SetMetadataServerUrlForTest(reader.get(), metadata_server->GetUrl() + "/");
  }

  std::unique_ptr<Configuration> CreateConfig() override {
    return std::unique_ptr<Configuration>(
      new Configuration(std::istringstream(
        "ProjectId: TestProjectId\n"
        "KubernetesClusterName: TestClusterName\n"
        "KubernetesClusterLocation: TestClusterLocation\n"
        "MetadataApiResourceTypeSeparator: \".\"\n"
        "KubernetesEndpointHost: https://kubernetes.host\n"
      )));
  }

  std::unique_ptr<testing::FakeServer> metadata_server;
};

TEST_F(KubernetesTestNoInstance, RegionalClusterAndFullResourceName) {
  const std::string cluster_full_name =
      "//container.googleapis.com/projects/TestProjectId/locations/"
      "TestClusterLocation/clusters/TestClusterName";

  EXPECT_EQ(cluster_full_name, ClusterFullName(*reader));
  EXPECT_EQ(
      cluster_full_name + "/k8s/namespaces/ns",
      FullResourceName(*reader, "/api/v1/namespaces/ns"));
  EXPECT_EQ(
      cluster_full_name + "/k8s/nodes/node-name",
      FullResourceName(*reader, "/api/v1/nodes/node-name"));
  EXPECT_EQ(
      cluster_full_name + "/k8s/namespaces/ns/pods/pod-name",
      FullResourceName(*reader, "/api/v1/namespaces/ns/pods/pod-name"));
  EXPECT_EQ(
      cluster_full_name + "/k8s/namespaces/ns/apps/deployments/dep-name",
      FullResourceName(
          *reader, "/apis/apps/v1beta1/namespaces/ns/deployments/dep-name"));
}

class KubernetesTestWithZonalCluster : public KubernetesTestNoInstance {
 protected:
  std::unique_ptr<Configuration> CreateConfig() override {
    return std::unique_ptr<Configuration>(
      new Configuration(std::istringstream(
        "ProjectId: TestProjectId\n"
        "KubernetesClusterLocation: us-central1-a\n"
        "KubernetesClusterName: TestClusterName\n"
      )));
  }
};

TEST_F(KubernetesTestWithZonalCluster, ZonalClusterAndFullResourceName) {
  const std::string cluster_full_name =
      "//container.googleapis.com/projects/TestProjectId/zones/us-central1-a/"
      "clusters/TestClusterName";

  EXPECT_EQ(cluster_full_name, ClusterFullName(*reader));
  EXPECT_EQ(
      cluster_full_name + "/k8s/nodes/node-name",
      FullResourceName(*reader, "/api/v1/nodes/node-name"));
}

TEST_F(KubernetesTestNoInstance, GetWatchPath) {
  EXPECT_EQ("/api/v1/watch/nodes",
            GetWatchPath(*reader, "nodes", "v1", ""));
  EXPECT_EQ("/api/v1/watch/nodes/node-name",
            GetWatchPath(*reader, "nodes", "v1", "/node-name"));
  EXPECT_EQ("/api/v1/watch/nodes?selector=Name%3Dname",
            GetWatchPath(*reader, "nodes", "v1", "?selector=Name%3Dname"));
  EXPECT_EQ("/apis/apps/v1/watch/deployments",
            GetWatchPath(*reader, "deployments", "apps/v1", ""));
}

TEST_F(KubernetesTestNoInstance, GetObjectMetadataService) {
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
  const auto m = GetObjectMetadata(*reader, service->As<json::Object>(),
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

TEST_F(KubernetesTestNoInstance, GetObjectMetadataEndpoints) {
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
  const auto m = GetObjectMetadata(*reader, service->As<json::Object>(),
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

TEST_F(KubernetesTestNoInstance, GetNodeMetadata) {
  const std::string node_name = "testname";
  json::value node = json::object({
    {"apiVersion", json::string("NodeVersion")},
    {"kind", json::string("Node")},
    {"metadata", json::object({
      {"name", json::string(node_name)},
      {"selfLink", json::string("/api/v1/nodes/testname")},
      {"creationTimestamp", json::string("2018-03-03T01:23:45.678901234Z")},
    })},
  });
  const auto m =
      GetNodeMetadata(*reader, node->As<json::Object>(), Timestamp(), false);
  EXPECT_EQ(1, m.ids().size());
  EXPECT_EQ("k8s_node."+node_name, m.ids()[0]);
  EXPECT_EQ(MonitoredResource("k8s_node", {
    {"cluster_name", "TestClusterName"},
    {"node_name", node_name},
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

TEST_F(KubernetesTestNoInstance, GetLegacyResource) {
  json::value pod = json::object({
    {"apiVersion", json::string("PodVersion")},
    {"kind", json::string("Pod")},
    {"metadata", json::object({
      {"namespace", json::string("TestNamespace")},
      {"name", json::string("TestName")},
      {"selfLink",
       json::string("/api/PodVersion/namespaces/TestNamespace/pods/TestName")
      },
      {"uid", json::string("TestUid")},
    })},
  });
  EXPECT_THROW(
      GetLegacyResource(*reader, pod->As<json::Object>(), "TestContainerName"),
      std::out_of_range);
}

TEST_F(KubernetesTestNoInstance, GetPodAndContainerMetadata) {
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
      *reader, pod->As<json::Object>(), Timestamp(), false);
  EXPECT_EQ(2, m.size());
  EXPECT_EQ(std::vector<std::string>({
    "k8s_container.TestPodUid.TestContainerName0",
    "k8s_container.TestNamespace.TestPodName.TestContainerName0"
  }), m[0].ids());
  EXPECT_EQ(MonitoredResource("k8s_container", {
    {"cluster_name", "TestClusterName"},
    {"container_name", "TestContainerName0"},
    {"location", "TestClusterLocation"},
    {"namespace_name", "TestNamespace"},
    {"pod_name", "TestPodName"},
  }), m[0].resource());
  EXPECT_TRUE(m[0].metadata().ignore);

  EXPECT_EQ(std::vector<std::string>({
    "k8s_pod.TestPodUid",
    "k8s_pod.TestNamespace.TestPodName"
  }), m[1].ids());
  EXPECT_EQ(MonitoredResource("k8s_pod", {
      {"cluster_name", "TestClusterName"},
      {"location", "TestClusterLocation"},
      {"namespace_name", "TestNamespace"},
      {"pod_name", "TestPodName"},
  }), m[1].resource());
  EXPECT_FALSE(m[1].metadata().ignore);
  EXPECT_EQ("PodVersion", m[1].metadata().version);
  EXPECT_FALSE(m[1].metadata().is_deleted);
  EXPECT_EQ(Timestamp(), m[1].metadata().collected_at);
  EXPECT_EQ(pod->ToString(), m[1].metadata().metadata->ToString());
}

class KubernetesTestWithInstance : public KubernetesTestNoInstance {
 protected:
  std::unique_ptr<Configuration> CreateConfig() override {
    return std::unique_ptr<Configuration>(
      new Configuration(std::istringstream(
        "InstanceId: TestID\n"
        "InstanceZone: TestZone\n"
        "ProjectId: TestProjectId\n"
        "KubernetesClusterLocation: TestClusterLocation\n"
        "KubernetesClusterName: TestClusterName\n"
      )));
  }
};

TEST_F(KubernetesTestWithInstance, GetNodeMetadata) {
  const std::string node_name = "testname";
  json::value node = json::object({
    {"apiVersion", json::string("NodeVersion")},
    {"kind", json::string("Node")},
    {"metadata", json::object({
      {"name", json::string(node_name)},
      {"selfLink", json::string("/api/v1/nodes/testname")},
      {"creationTimestamp", json::string("2018-03-03T01:23:45.678901234Z")},
    })},
  });
  const auto m =
      GetNodeMetadata(*reader, node->As<json::Object>(), Timestamp(), false);
  EXPECT_EQ(1, m.ids().size());
  EXPECT_EQ("k8s_node."+node_name, m.ids()[0]);
  EXPECT_EQ(MonitoredResource("k8s_node", {
    {"cluster_name", "TestClusterName"},
    {"node_name", node_name},
    {"location", "TestClusterLocation"},
  }), m.resource());
  EXPECT_EQ("NodeVersion", m.metadata().version);
  EXPECT_FALSE(m.metadata().is_deleted);
  EXPECT_EQ(Timestamp(), m.metadata().collected_at);
  EXPECT_EQ(node->ToString(), m.metadata().metadata->ToString());
}

TEST_F(KubernetesTestWithInstance, GetPodMetadata) {
  const std::string pod_name = "TestName";
  json::value pod = json::object({
    {"apiVersion", json::string("PodVersion")},
    {"kind", json::string("Pod")},
    {"metadata", json::object({
      {"namespace", json::string("TestNamespace")},
      {"name", json::string(pod_name)},
      {"selfLink",
       json::string("/api/v1/namespaces/TestNamespace/pods/TestName")},
      {"uid", json::string("TestUid")},
      {"creationTimestamp", json::string("2018-03-03T01:23:45.678901234Z")},
    })},
  });
  const auto m = GetPodMetadata(*reader, pod->As<json::Object>(),
                                Timestamp(), false);

  EXPECT_EQ(std::vector<std::string>(
      {"k8s_pod.TestUid", "k8s_pod.TestNamespace."+pod_name}), m.ids());
  EXPECT_EQ(MonitoredResource("k8s_pod", {
    {"cluster_name", "TestClusterName"},
    {"pod_name", pod_name},
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

TEST_F(KubernetesTestWithInstance, GetLegacyResource) {
  json::value pod = json::object({
    {"apiVersion", json::string("PodVersion")},
    {"kind", json::string("Pod")},
    {"metadata", json::object({
      {"namespace", json::string("TestNamespace")},
      {"name", json::string("TestName")},
      {"selfLink",
       json::string("/api/PodVersion/namespaces/TestNamespace/pods/TestName")},
      {"uid", json::string("TestUid")},
    })},
  });
  const auto m = GetLegacyResource(*reader, pod->As<json::Object>(),
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

TEST_F(KubernetesTestWithInstance, GetContainerMetadata) {
  json::value pod = json::object({
    {"apiVersion", json::string("PodVersion")},
    {"kind", json::string("Pod")},
    {"metadata", json::object({
      {"namespace", json::string("TestNamespace")},
      {"name", json::string("TestName")},
      {"selfLink",
       json::string("/api/PodVersion/namespaces/TestNamespace/pods/TestName")},
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
      *reader,
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

TEST_F(KubernetesTestWithInstance, GetPodAndContainerMetadata) {
  const std::string pod_name = "TestPodName";
  json::value pod = json::object({
    {"apiVersion", json::string("PodVersion")},
    {"kind", json::string("Pod")},
    {"metadata", json::object({
      {"name", json::string(pod_name)},
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
      *reader, pod->As<json::Object>(), Timestamp(), false);
  EXPECT_EQ(3, m.size());
  EXPECT_EQ(std::vector<std::string>({
    "gke_container.TestNamespace.TestPodUid.TestContainerName0",
    "gke_container.TestNamespace."+pod_name+".TestContainerName0",
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
    "k8s_container.TestNamespace."+pod_name+".TestContainerName0"
  }), m[1].ids());
  EXPECT_EQ(MonitoredResource("k8s_container", {
    {"cluster_name", "TestClusterName"},
    {"container_name", "TestContainerName0"},
    {"location", "TestClusterLocation"},
    {"namespace_name", "TestNamespace"},
    {"pod_name", pod_name},
  }), m[1].resource());
  EXPECT_TRUE(m[1].metadata().ignore);

  EXPECT_EQ(std::vector<std::string>({
    "k8s_pod.TestPodUid",
    "k8s_pod.TestNamespace."+pod_name
  }), m[2].ids());
  EXPECT_EQ(MonitoredResource("k8s_pod", {
      {"cluster_name", "TestClusterName"},
      {"location", "TestClusterLocation"},
      {"namespace_name", "TestNamespace"},
      {"pod_name", pod_name},
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

class KubernetesTestSecrets : public KubernetesTest {
 protected:
  std::unique_ptr<Configuration> CreateConfig() override {
    return std::unique_ptr<Configuration>(new Configuration());
  }
};

TEST_F(KubernetesTestSecrets, KubernetesApiToken) {
  testing::TemporaryFile token_file("token", "the-api-token");

  SetServiceAccountDirectoryForTest(
      reader.get(), token_file.FullPath().parent_path().native());

  EXPECT_EQ("the-api-token", KubernetesApiToken(*reader));

  // Check that the value is cached.
  token_file.SetContents("updated-api-token");
  EXPECT_EQ("the-api-token", KubernetesApiToken(*reader));
}

TEST_F(KubernetesTestSecrets, KubernetesNamespace) {
  testing::TemporaryFile namespace_file("namespace", "the-namespace");

  SetServiceAccountDirectoryForTest(
      reader.get(), namespace_file.FullPath().parent_path().native());

  EXPECT_EQ("the-namespace", KubernetesNamespace(*reader));

  // Check that the value is cached.
  namespace_file.SetContents("updated-namespace");
  EXPECT_EQ("the-namespace", KubernetesNamespace(*reader));
}

class KubernetesTestFakeServer : public KubernetesTest {
 protected:
  void SetUp() override {
    server.reset(new testing::FakeServer());
    KubernetesTest::SetUp();
  }

  std::unique_ptr<Configuration> CreateConfig() override {
    return std::unique_ptr<Configuration>(
      new Configuration(std::istringstream(
        "InstanceId: TestID\n"
        "InstanceZone: TestZone\n"
        "KubernetesClusterLocation: TestClusterLocation\n"
        "KubernetesClusterName: TestClusterName\n"
        "KubernetesEndpointHost: " + server->GetUrl() + "\n"
        "KubernetesNodeName: TestNodeName\n"
      )));
  }

  std::unique_ptr<testing::FakeServer> server;
};

TEST_F(KubernetesTestFakeServer, QueryMaster) {
  server->SetResponse("/a/b/c", "{\"hello\":\"world\"}");

  EXPECT_EQ(QueryMaster(*reader, "/a/b/c")->ToString(), "{\"hello\":\"world\"}");

  EXPECT_THROW(QueryMaster(*reader, "/d/e/f"), QueryException);
}

TEST_F(KubernetesTestFakeServer, MetadataQuery) {
  const std::string node_name = "TestNodeName";
  json::value node = json::object({
    {"apiVersion", json::string("NodeVersion")},
    {"kind", json::string("Node")},
    {"metadata", json::object({
      {"name", json::string("TestNodeName")},
      {"selfLink", json::string("/api/v1/nodes/TestNodeName")},
      {"creationTimestamp", json::string("2018-03-03T01:23:45.678901234Z")},
    })},
  });
  const std::string pod_name = "TestPodName";
  json::value a_pod = json::object({
    {"apiVersion", json::string("PodVersion")},
    {"kind", json::string("Pod")},
    {"metadata", json::object({
      {"namespace", json::string("TestNamespace")},
      {"name", json::string(pod_name)},
      {"selfLink",
       json::string("/api/v1/namespaces/TestNamespace/pods/"+pod_name)},
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
  });
  json::value pod_list = json::object({
    {"apiVersion", json::string("1.2.3")},
    {"items", json::array({
        a_pod->As<json::Object>()->Clone(),
    })},
  });
  server->SetResponse("/api/v1/nodes/TestNodeName", node->ToString());
  server->SetResponse("/api/v1/pods?fieldSelector=spec.nodeName%3DTestNodeName",
                      pod_list->ToString());

  std::vector<KubernetesUpdater::ResourceMetadata> m = reader->MetadataQuery();
  EXPECT_EQ(4, m.size());

  // Verify node metadata.
  EXPECT_EQ(1, m[0].ids().size());
  EXPECT_EQ("k8s_node."+node_name, m[0].ids()[0]);
  EXPECT_EQ(MonitoredResource("k8s_node", {
    {"cluster_name", "TestClusterName"},
    {"node_name", node_name},
    {"location", "TestClusterLocation"},
  }), m[0].resource());
  EXPECT_EQ("NodeVersion", m[0].metadata().version);
  EXPECT_FALSE(m[0].metadata().is_deleted);
  EXPECT_EQ(node->ToString(), m[0].metadata().metadata->ToString());

  // Verify pod metadata.
  EXPECT_EQ(std::vector<std::string>({
    "gke_container.TestNamespace.TestPodUid.TestContainerName0",
    "gke_container.TestNamespace."+pod_name+".TestContainerName0",
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
    "k8s_container.TestNamespace."+pod_name+".TestContainerName0"
  }), m[2].ids());
  EXPECT_EQ(MonitoredResource("k8s_container", {
    {"cluster_name", "TestClusterName"},
    {"container_name", "TestContainerName0"},
    {"location", "TestClusterLocation"},
    {"namespace_name", "TestNamespace"},
    {"pod_name", pod_name},
  }), m[2].resource());
  EXPECT_TRUE(m[2].metadata().ignore);

  EXPECT_EQ(std::vector<std::string>({
    "k8s_pod.TestPodUid",
    "k8s_pod.TestNamespace."+pod_name
  }), m[3].ids());
  EXPECT_EQ(MonitoredResource("k8s_pod", {
      {"cluster_name", "TestClusterName"},
      {"location", "TestClusterLocation"},
      {"namespace_name", "TestNamespace"},
      {"pod_name", pod_name},
  }), m[3].resource());
  EXPECT_FALSE(m[3].metadata().ignore);
  EXPECT_EQ("PodVersion", m[3].metadata().version);
  EXPECT_FALSE(m[3].metadata().is_deleted);
  EXPECT_EQ(a_pod->ToString(), m[3].metadata().metadata->ToString());
}

class KubernetesTestFakeServerConfigurable : public KubernetesTestFakeServer {
 protected:
  virtual std::string ExtraConfig() {
    return "";
  }
  std::unique_ptr<Configuration> CreateConfig() override {
    return std::unique_ptr<Configuration>(
      new Configuration(std::istringstream(
        "InstanceId: TestID\n"
        "InstanceResourceType: gce_instance\n"
        "InstanceZone: TestZone\n"
        "ProjectId: TestProjectId\n"
        "KubernetesClusterLocation: TestClusterLocation\n"
        "KubernetesClusterName: TestClusterName\n"
        "KubernetesEndpointHost: " + server->GetUrl() + "\n"
        "KubernetesNodeName: TestNodeName\n"
        "KubernetesUseWatch: true\n"
        + ExtraConfig()
      )));
  }
};

class KubernetesTestFakeServerOneWatchRetryNodeLevelMetadata
    : public KubernetesTestFakeServerConfigurable {
 protected:
  std::string ExtraConfig() override {
    return
      "KubernetesClusterLevelMetadata: false\n"
      "KubernetesUpdaterWatchConnectionRetries: 1\n";
  }
};

class KubernetesTestFakeServerOneWatchRetryClusterLevelMetadata
    : public KubernetesTestFakeServerConfigurable {
 protected:
  std::string ExtraConfig() override {
    return
      "KubernetesClusterLevelMetadata: true\n"
      "KubernetesUpdaterWatchConnectionRetries: 1\n";
  }
};

class KubernetesTestFakeServerThreeWatchRetriesNodeLevelMetadata
    : public KubernetesTestFakeServerConfigurable {
 protected:
  std::string ExtraConfig() override {
    return
      "KubernetesClusterLevelMetadata: false\n"
      "KubernetesUpdaterWatchConnectionRetries: 3\n";
  }
};

namespace {

// Polls store until collected_at for resource is newer than
// last_timestamp.  Returns false if newer timestamp not found after 3
// seconds (polling every 100 millis).
bool WaitForNewerCollectionTimestamp(const MetadataStore& store,
                                     const std::string& name,
                                     const std::string& version,
                                     Timestamp last_timestamp) {
  for (int i = 0; i < 30; i++){
    for (auto& m: store.GetMetadata()) {
      if (m.name == name && m.version == version &&
          m.collected_at > last_timestamp) {
        return true;
      }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
  return false;
}

MetadataStore::Metadata FindMetadata(const MetadataStore& store,
                                      const std::string& name,
                                      const std::string& version) {
  for (auto& m: store.GetMetadata()) {
    if (m.name == name && m.version == version) {
      return std::move(m);
    }
  }

  return MetadataStore::Metadata::IGNORED();
}

// Wait for updater's node watcher to connect to the server (hanging
// GET), then send 3 stream responses (2 adds and 1 delete) from the
// fake Kubernetes master and verify that the updater propagates them
// to the store.
void TestNodes(testing::FakeServer& server, MetadataStore& store,
               const std::string& nodes_watch_path) {
  const auto timeout = time::seconds(3);
  ASSERT_TRUE(server.WaitForMinTotalConnections(nodes_watch_path, 1, timeout));
  const std::string node1_name = "TestNodeName1";
  json::value node1 = json::object({
    {"metadata", json::object({
      {"name", json::string(node1_name)},
      {"creationTimestamp", json::string("2018-03-03T01:23:45.678901234Z")},
    })},
  });
  const std::string node2_name = "TestNodeName2";
  json::value node2 = json::object({
    {"metadata", json::object({
      {"name", json::string(node2_name)},
      {"creationTimestamp", json::string("2018-03-03T01:23:45.678901234Z")},
    })},
  });
  MonitoredResource resource1("k8s_node",
                              {{"cluster_name", "TestClusterName"},
                               {"location", "TestClusterLocation"},
                               {"node_name", node1_name}});
  MonitoredResource resource2("k8s_node",
                              {{"cluster_name", "TestClusterName"},
                               {"location", "TestClusterLocation"},
                               {"node_name", node2_name}});
  Timestamp last_timestamp = std::chrono::system_clock::now();

  // Add node #1 and wait until watcher has processed it (by polling the store).
  server.SendStreamResponse(nodes_watch_path, json::object({
    {"type", json::string("ADDED")},
    {"object", node1->Clone()},
  })->ToString());
  EXPECT_TRUE(WaitForNewerCollectionTimestamp(store, node1_name, "", last_timestamp));

  // Verify node #1 values in the store & update last_timestamp.
  EXPECT_EQ(resource1, store.LookupResource("k8s_node."+node1_name));
  {
    const auto& metadata = FindMetadata(store, node1_name, "");
    EXPECT_EQ("", metadata.version);
    EXPECT_FALSE(metadata.is_deleted);
    last_timestamp = metadata.collected_at;
  }

  // Add node #2 and wait until watcher has processed it (by polling the store).
  server.SendStreamResponse(nodes_watch_path, json::object({
    {"type", json::string("ADDED")},
    {"object", node2->Clone()},
  })->ToString());
  EXPECT_TRUE(WaitForNewerCollectionTimestamp(store, node2_name, "", last_timestamp));

  // Verify node #2 values in the store & update last_timestamp.
  EXPECT_EQ(resource2, store.LookupResource("k8s_node."+node2_name));
  {
    const auto& metadata = FindMetadata(store, node2_name, "");
    EXPECT_EQ("", metadata.version);
    EXPECT_FALSE(metadata.is_deleted);
    last_timestamp = metadata.collected_at;
  }

  // Delete node #1 and wait until watcher has processed it (by
  // polling the store).
  server.SendStreamResponse(nodes_watch_path, json::object({
    {"type", json::string("DELETED")},
    {"object", node1->Clone()},
  })->ToString());
  EXPECT_TRUE(WaitForNewerCollectionTimestamp(store, node1_name, "", last_timestamp));

  // Verify node #1 values in the store.
  EXPECT_EQ(resource1, store.LookupResource("k8s_node."+node1_name));
  {
    const auto& metadata = FindMetadata(store, node1_name, "");
    EXPECT_EQ("", metadata.version);
    EXPECT_TRUE(metadata.is_deleted);
  }
}

// Wait for updater's pod watcher to connect to the server (hanging
// GET), then send 3 stream responses (2 adds and 1 delete) from the
// fake Kubernetes master and verify that the updater propagates them
// to the store.
void TestPods(testing::FakeServer& server, MetadataStore& store,
              const std::string& pods_watch_path) {
  const auto timeout = time::seconds(3);
  ASSERT_TRUE(server.WaitForMinTotalConnections(pods_watch_path, 1, timeout));
  const std::string pod1_name = "TestPodName1";
  json::value pod1 = json::object({
    {"metadata", json::object({
      {"name", json::string(pod1_name)},
      {"namespace", json::string("TestNamespace")},
      {"uid", json::string("TestPodUid1")},
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
  const std::string pod2_name = "TestPodName2";
  json::value pod2 = json::object({
    {"metadata", json::object({
      {"name", json::string(pod2_name)},
      {"namespace", json::string("TestNamespace")},
      {"uid", json::string("TestPodUid2")},
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
  json::value spec = json::object({
    {"version", json::string("1.6")},  // Hard-coded in kubernetes.cc.
    {"raw", json::object({
      {"name", json::string("TestContainerName0")},
    })},
  });
  json::value status = json::object({
    {"version", json::string("1.6")},  // Hard-coded in kubernetes.cc.
    {"raw", json::object({
      {"name", json::string("TestContainerName0")},
    })},
  });
  MonitoredResource k8s_pod_resource1(
    "k8s_pod",
    {{"cluster_name", "TestClusterName"},
     {"location", "TestClusterLocation"},
     {"namespace_name", "TestNamespace"},
     {"pod_name", pod1_name}});
  MonitoredResource k8s_pod_resource2(
    "k8s_pod",
    {{"cluster_name", "TestClusterName"},
     {"location", "TestClusterLocation"},
     {"namespace_name", "TestNamespace"},
     {"pod_name", pod2_name}});
  MonitoredResource k8s_container_resource1(
    "k8s_container",
    {{"cluster_name", "TestClusterName"},
     {"container_name", "TestContainerName0"},
     {"location", "TestClusterLocation"},
     {"namespace_name", "TestNamespace"},
     {"pod_name", pod1_name}});
  MonitoredResource k8s_container_resource2(
    "k8s_container",
    {{"cluster_name", "TestClusterName"},
     {"container_name", "TestContainerName0"},
     {"location", "TestClusterLocation"},
     {"namespace_name", "TestNamespace"},
     {"pod_name", pod2_name}});
  Timestamp last_timestamp = std::chrono::system_clock::now();

  // Add pod #1 and wait until watch has processed it (by polling the store).
  server.SendStreamResponse(pods_watch_path, json::object({
    {"type", json::string("ADDED")},
    {"object", pod1->Clone()},
  })->ToString());
  EXPECT_TRUE(WaitForNewerCollectionTimestamp(
      store, pod1_name, "", last_timestamp));

  // Verify pod #1 values in the store & update last_timestamp.
  EXPECT_EQ(k8s_pod_resource1, store.LookupResource(
      "k8s_pod.TestNamespace."+pod1_name));
  EXPECT_EQ(k8s_pod_resource1, store.LookupResource(
      "k8s_pod.TestPodUid1"));
  EXPECT_EQ(k8s_container_resource1, store.LookupResource(
      "k8s_container.TestNamespace."+pod1_name+".TestContainerName0"));
  EXPECT_EQ(k8s_container_resource1, store.LookupResource(
      "k8s_container.TestPodUid1.TestContainerName0"));
  {
    const auto& k8s_pod_metadata = FindMetadata(store, pod1_name, "");
    EXPECT_EQ(k8s_pod_metadata.name, pod1_name);
    EXPECT_FALSE(k8s_pod_metadata.ignore);
    EXPECT_EQ("", k8s_pod_metadata.version);
    EXPECT_FALSE(k8s_pod_metadata.is_deleted);

    last_timestamp = k8s_pod_metadata.collected_at;
  }

  // Add pod #2 and wait until watch has processed it (by polling the store).
  server.SendStreamResponse(pods_watch_path, json::object({
    {"type", json::string("ADDED")},
    {"object", pod2->Clone()},
  })->ToString());
  EXPECT_TRUE(WaitForNewerCollectionTimestamp(
      store, pod2_name, "", last_timestamp));

  // Verify pod #2 values in the store & update last_timestamp.
  EXPECT_EQ(k8s_pod_resource2, store.LookupResource(
      "k8s_pod.TestNamespace."+pod2_name));
  EXPECT_EQ(k8s_pod_resource2, store.LookupResource(
      "k8s_pod.TestPodUid2"));
  EXPECT_EQ(k8s_container_resource2, store.LookupResource(
      "k8s_container.TestNamespace."+pod2_name+".TestContainerName0"));
  EXPECT_EQ(k8s_container_resource2, store.LookupResource(
      "k8s_container.TestPodUid2.TestContainerName0"));
  {
    const auto& k8s_pod_metadata = FindMetadata(store, pod2_name, "");
    EXPECT_EQ(k8s_pod_metadata.name, pod2_name);
    EXPECT_FALSE(k8s_pod_metadata.ignore);
    EXPECT_EQ("", k8s_pod_metadata.version);
    EXPECT_FALSE(k8s_pod_metadata.is_deleted);

    last_timestamp = k8s_pod_metadata.collected_at;
  }

  // Delete pod #1 and wait until watch has processed it (by polling the store).
  server.SendStreamResponse(pods_watch_path, json::object({
    {"type", json::string("DELETED")},
    {"object", pod1->Clone()},
  })->ToString());
  EXPECT_TRUE(WaitForNewerCollectionTimestamp(
      store, pod1_name, "", last_timestamp));

  // Verify pod #1 values in the store.
  EXPECT_EQ(k8s_pod_resource1, store.LookupResource(
      "k8s_pod.TestNamespace."+pod1_name));
  EXPECT_EQ(k8s_pod_resource1, store.LookupResource(
      "k8s_pod.TestPodUid1"));
  EXPECT_EQ(k8s_container_resource1, store.LookupResource(
      "k8s_container.TestNamespace."+pod1_name+".TestContainerName0"));
  EXPECT_EQ(k8s_container_resource1, store.LookupResource(
      "k8s_container.TestPodUid1.TestContainerName0"));
  {
    const auto& k8s_pod_metadata = FindMetadata(store, pod1_name, "");
    EXPECT_EQ(k8s_pod_metadata.name, pod1_name);
    EXPECT_FALSE(k8s_pod_metadata.ignore);
    EXPECT_EQ("", k8s_pod_metadata.version);
    EXPECT_TRUE(k8s_pod_metadata.is_deleted);
  }
}

}  // namespace

TEST_F(KubernetesTestFakeServerOneWatchRetryNodeLevelMetadata,
       KubernetesUpdater) {
  const std::string nodes_watch_path =
    "/api/v1/watch/nodes/TestNodeName";
  const std::string pods_watch_path =
    "/api/v1/watch/pods?fieldSelector=spec.nodeName%3DTestNodeName";

  // Create a fake server representing the Kubernetes master.
  server->SetResponse("/api/v1/nodes?limit=1", "{}");
  server->SetResponse("/api/v1/pods?limit=1", "{}");
  server->AllowStream(pods_watch_path);
  server->AllowStream(nodes_watch_path);

  MetadataStore store(*config);
  KubernetesUpdater updater(*config, /*health_checker=*/nullptr, &store);
  updater.Start();

  // Wait for updater's watchers to connect to the server (hanging GETs).
  const auto timeout = time::seconds(3);
  ASSERT_TRUE(server->WaitForMinTotalConnections(nodes_watch_path, 1, timeout));
  ASSERT_TRUE(server->WaitForMinTotalConnections(pods_watch_path, 1, timeout));

  // For nodes, send stream responses from the fake Kubernetes
  // master and verify that the updater propagates them to the store.
  // Do 3 updates to test multiple updates being pushed over the
  // hanging GET.
  Timestamp last_nodes_timestamp = std::chrono::system_clock::now();
  for (int i = 0; i < 3; i++) {
    json::value node_metadata = json::object({
      {"type", json::string("ADDED")},
      {"object", json::object({
        {"apiVersion", json::string("NodeVersion")},
        {"kind", json::string("Node")},
        {"metadata", json::object({
          {"name", json::string("TestNodeName")},
          {"selfLink", json::string("/api/NodeVersion/nodes/TestNodeName")},
          {"creationTimestamp", json::string("2018-03-03T01:23:45.678901234Z")},
        })}
      })}
    });
    // Send a response to the watcher.
    server->SendStreamResponse(
        "/api/v1/watch/nodes/TestNodeName", node_metadata->ToString());
    // Wait until watcher has processed response (by polling the store).
    const std::string name =
        "//container.googleapis.com/projects/TestProjectId/locations/"
        "TestClusterLocation/clusters/TestClusterName/k8s/nodes/TestNodeName";
    const std::string version = "NodeVersion";
    EXPECT_TRUE(
        WaitForNewerCollectionTimestamp(
            store, name, version, last_nodes_timestamp)
    );
    for (auto& m: store.GetMetadata()) {
      if (m.name == name && m.version == version) {
        // TODO: Insert tests of metadata values.
        last_nodes_timestamp = m.collected_at;
      }
    }
  }

  // For pods, do the same thing.
  Timestamp last_pods_timestamp = std::chrono::system_clock::now();
  for (int i = 0; i < 3; i++) {
    json::value pod_metadata = json::object({
      {"type", json::string("ADDED")},
      {"object", json::object({
        {"apiVersion", json::string("PodVersion")},
        {"kind", json::string("Pod")},
        {"metadata", json::object({
          {"name", json::string("TestPodName")},
          {"selfLink",
           json::string("/api/PodVersion/namespaces/TestNamespace/pods/"
                        "TestPodName")},
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
      })}
    });
    // Send a response to the watcher.
    server->SendStreamResponse(
        "/api/v1/watch/pods?fieldSelector=spec.nodeName%3DTestNodeName",
        pod_metadata->ToString());
    // Wait until watcher has processed response (by polling the store).
    const std::string name =
        "//container.googleapis.com/projects/TestProjectId/locations/"
        "TestClusterLocation/clusters/TestClusterName/k8s/namespaces/"
        "TestNamespace/pods/TestPodName";
    const std::string version = "PodVersion";
    EXPECT_TRUE(
        WaitForNewerCollectionTimestamp(
            store, name, version, last_pods_timestamp)
    );
  }

  // Terminate the hanging GETs on the server so that the updater will finish.
  server->TerminateAllStreams();
}

TEST_F(KubernetesTestFakeServerThreeWatchRetriesNodeLevelMetadata,
       KubernetesUpdaterReconnection) {
  const std::string nodes_watch_path =
    "/api/v1/watch/nodes/TestNodeName";
  const std::string pods_watch_path =
    "/api/v1/watch/pods?fieldSelector=spec.nodeName%3DTestNodeName";
  const auto timeout = time::seconds(3);

  // Create a fake server representing the Kubernetes master.
  server->SetResponse("/api/v1/nodes?limit=1", "{}");
  server->SetResponse("/api/v1/pods?limit=1", "{}");
  server->AllowStream(nodes_watch_path);
  server->AllowStream(pods_watch_path);

  MetadataStore store(*config);
  KubernetesUpdater updater(*config, /*health_checker=*/nullptr, &store);
  updater.Start();

  // Step 1: Wait for initial connection from watchers.
  server->WaitForMinTotalConnections(nodes_watch_path, 1, timeout);
  server->WaitForMinTotalConnections(pods_watch_path, 1, timeout);

  // Step 2: Terminate all streams and wait for watchers to reconnect.
  server->TerminateAllStreams();
  server->WaitForMinTotalConnections(nodes_watch_path, 2, timeout);
  server->WaitForMinTotalConnections(pods_watch_path, 2, timeout);

  // Step 3: Terminate again and wait for final reconnection
  // (configuration specifies 3 retries).
  server->TerminateAllStreams();
  server->WaitForMinTotalConnections(nodes_watch_path, 3, timeout);
  server->WaitForMinTotalConnections(pods_watch_path, 3, timeout);

  // Terminate the hanging GETs on the server so that the updater will finish.
  server->TerminateAllStreams();
}

namespace {
class FakeKubernetesUpdater : public KubernetesUpdater {
 public:
  FakeKubernetesUpdater(const Configuration& config,
                        HealthChecker* health_checker,
                        MetadataStore* store)
    : KubernetesUpdater(
          config, health_checker, store,
          DelayTimerFactoryImpl<testing::FakeClock>::New()) {}
};
}

TEST_F(KubernetesTestFakeServerThreeWatchRetriesNodeLevelMetadata,
       KubernetesUpdaterHourlyReconnection) {
  const std::string nodes_watch_path =
    "/api/v1/watch/nodes/TestNodeName";
  const std::string pods_watch_path =
    "/api/v1/watch/pods?fieldSelector=spec.nodeName%3DTestNodeName";
  const auto timeout = time::seconds(3);

  // Create a fake server representing the Kubernetes master.
  server->SetResponse("/api/v1/nodes?limit=1", "{}");
  server->SetResponse("/api/v1/pods?limit=1", "{}");
  server->AllowStream(nodes_watch_path);
  server->AllowStream(pods_watch_path);

  MetadataStore store(*config);
  FakeKubernetesUpdater updater(*config, /*health_checker=*/nullptr, &store);
  updater.Start();

  // Wait for connection #1.
  ASSERT_TRUE(server->WaitForMinTotalConnections(nodes_watch_path, 1, timeout));
  ASSERT_TRUE(server->WaitForMinTotalConnections(pods_watch_path, 1, timeout));

  // Advance fake clock only 30 minutes, not enough to trigger reconnection.
  testing::FakeClock::Advance(std::chrono::seconds(1800));
  EXPECT_EQ(1, server->NumWatchers(nodes_watch_path));
  EXPECT_EQ(1, server->NumWatchers(pods_watch_path));

  // Advance another 30 minutes to trigger connection #2.
  testing::FakeClock::Advance(std::chrono::seconds(1800));
  ASSERT_TRUE(server->WaitForMinTotalConnections(nodes_watch_path, 2, timeout));
  ASSERT_TRUE(server->WaitForMinTotalConnections(pods_watch_path, 2, timeout));

  // Advance 60 minutes to trigger connection #3.
  testing::FakeClock::Advance(std::chrono::seconds(3600));
  ASSERT_TRUE(server->WaitForMinTotalConnections(nodes_watch_path, 3, timeout));
  ASSERT_TRUE(server->WaitForMinTotalConnections(pods_watch_path, 3, timeout));

  // Terminate the hanging GETs on the server so that the updater will finish.
  server->TerminateAllStreams();
}

}  // namespace google
