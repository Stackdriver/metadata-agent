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
        "KubernetesClusterName: TestClusterName\n"
        "KubernetesClusterLocation: TestClusterLocation\n"
        "MetadataIngestionRawContentVersion: \n"
      )));
  }

  std::unique_ptr<testing::FakeServer> metadata_server;
};

TEST_F(KubernetesTestNoInstance, GetNodeMetadata) {
  json::value node = json::object({
    {"metadata", json::object({
      {"name", json::string("testname")},
      {"creationTimestamp", json::string("2018-03-03T01:23:45.678901234Z")},
    })},
  });
  const auto m =
      GetNodeMetadata(*reader, node->As<json::Object>(), Timestamp(), false);
  EXPECT_EQ(1, m.ids().size());
  EXPECT_EQ("k8s_node.testname", m.ids()[0]);
  EXPECT_EQ(MonitoredResource("k8s_node", {
    {"cluster_name", "TestClusterName"},
    {"node_name", "testname"},
    {"location", "TestClusterLocation"},
  }), m.resource());
  EXPECT_EQ("", m.metadata().version);
  EXPECT_FALSE(m.metadata().is_deleted);
  EXPECT_EQ(Timestamp(), m.metadata().collected_at);
  json::value node_metadata = json::object({
    {"blobs", json::object({
      {"api", json::object({
        {"version", json::string("1.6")},  // Hard-coded in kubernetes.cc.
        {"raw", std::move(node)},
      })},
    })},
  });
  EXPECT_EQ(node_metadata->ToString(), m.metadata().metadata->ToString());
}

TEST_F(KubernetesTestNoInstance, GetLegacyResource) {
  json::value pod = json::object({
    {"metadata", json::object({
      {"namespace", json::string("TestNamespace")},
      {"name", json::string("TestName")},
      {"uid", json::string("TestUid")},
    })},
  });
  EXPECT_THROW(
      GetLegacyResource(*reader, pod->As<json::Object>(), "TestContainerName"),
      std::out_of_range);
}

TEST_F(KubernetesTestNoInstance, GetPodAndContainerMetadata) {
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
  EXPECT_EQ("", m[1].metadata().version);
  EXPECT_FALSE(m[1].metadata().is_deleted);
  EXPECT_EQ(Timestamp(), m[1].metadata().collected_at);
  json::value pod_metadata = json::object({
    {"blobs", json::object({
      {"api", json::object({
        {"version", json::string("1.6")},  // Hard-coded in kubernetes.cc.
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
      })},
    })},
  });
  EXPECT_EQ(pod_metadata->ToString(),
            m[1].metadata().metadata->ToString());
}

class KubernetesTestWithInstance : public KubernetesTestNoInstance {
 protected:
  std::unique_ptr<Configuration> CreateConfig() override {
    return std::unique_ptr<Configuration>(
      new Configuration(std::istringstream(
        "InstanceId: TestID\n"
        "InstanceZone: TestZone\n"
        "KubernetesClusterLocation: TestClusterLocation\n"
        "KubernetesClusterName: TestClusterName\n"
        "MetadataIngestionRawContentVersion: \n"
      )));
  }
};

TEST_F(KubernetesTestWithInstance, GetNodeMetadata) {
  json::value node = json::object({
    {"metadata", json::object({
      {"name", json::string("testname")},
      {"creationTimestamp", json::string("2018-03-03T01:23:45.678901234Z")},
    })},
  });
  const auto m =
      GetNodeMetadata(*reader, node->As<json::Object>(), Timestamp(), false);
  EXPECT_EQ(1, m.ids().size());
  EXPECT_EQ("k8s_node.testname", m.ids()[0]);
  EXPECT_EQ(MonitoredResource("k8s_node", {
    {"cluster_name", "TestClusterName"},
    {"node_name", "testname"},
    {"location", "TestClusterLocation"},
  }), m.resource());
  EXPECT_EQ("", m.metadata().version);
  EXPECT_FALSE(m.metadata().is_deleted);
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

TEST_F(KubernetesTestWithInstance, GetPodMetadata) {
  json::value pod = json::object({
    {"metadata", json::object({
      {"namespace", json::string("TestNamespace")},
      {"name", json::string("TestName")},
      {"uid", json::string("TestUid")},
      {"creationTimestamp", json::string("2018-03-03T01:23:45.678901234Z")},
    })},
  });
  const auto m = GetPodMetadata(*reader, pod->As<json::Object>(),
                                Timestamp(), false);

  EXPECT_EQ(std::vector<std::string>(
      {"k8s_pod.TestUid", "k8s_pod.TestNamespace.TestName"}), m.ids());
  EXPECT_EQ(MonitoredResource("k8s_pod", {
    {"cluster_name", "TestClusterName"},
    {"pod_name", "TestName"},
    {"location", "TestClusterLocation"},
    {"namespace_name", "TestNamespace"},
  }), m.resource());
  EXPECT_EQ("", m.metadata().version);
  EXPECT_FALSE(m.metadata().is_deleted);
  EXPECT_EQ(Timestamp(), m.metadata().collected_at);
  EXPECT_FALSE(m.metadata().ignore);
  json::value expected_metadata = json::object({
    {"blobs", json::object({
      {"api", json::object({
        {"version", json::string("1.6")},  // Hard-coded in kubernetes.cc.
        {"raw", json::object({
          {"metadata", json::object({
            {"creationTimestamp",
             json::string("2018-03-03T01:23:45.678901234Z")},
            {"name", json::string("TestName")},
            {"namespace", json::string("TestNamespace")},
            {"uid", json::string("TestUid")},
          })},
        })},
      })},
    })},
  });
  EXPECT_EQ(expected_metadata->ToString(), m.metadata().metadata->ToString());
}

TEST_F(KubernetesTestWithInstance, GetLegacyResource) {
  json::value pod = json::object({
    {"metadata", json::object({
      {"namespace", json::string("TestNamespace")},
      {"name", json::string("TestName")},
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
      *reader, pod->As<json::Object>(), Timestamp(), false);
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
  EXPECT_EQ("", m[2].metadata().version);
  EXPECT_FALSE(m[2].metadata().is_deleted);
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
        "MetadataIngestionRawContentVersion: \n"
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
  json::value node = json::object({
    {"metadata", json::object({
      {"name", json::string("TestNodeName")},
      {"creationTimestamp", json::string("2018-03-03T01:23:45.678901234Z")},
    })},
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
  server->SetResponse("/api/v1/nodes/TestNodeName", node->ToString());
  server->SetResponse("/api/v1/pods?fieldSelector=spec.nodeName%3DTestNodeName",
                      pod->ToString());

  std::vector<KubernetesUpdater::ResourceMetadata> m = reader->MetadataQuery();
  EXPECT_EQ(4, m.size());

  // Verify node metadata.
  EXPECT_EQ(1, m[0].ids().size());
  EXPECT_EQ("k8s_node.TestNodeName", m[0].ids()[0]);
  EXPECT_EQ(MonitoredResource("k8s_node", {
    {"cluster_name", "TestClusterName"},
    {"node_name", "TestNodeName"},
    {"location", "TestClusterLocation"},
  }), m[0].resource());
  EXPECT_EQ("", m[0].metadata().version);
  EXPECT_FALSE(m[0].metadata().is_deleted);
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
  EXPECT_EQ("", m[3].metadata().version);
  EXPECT_FALSE(m[3].metadata().is_deleted);
  json::value pod_metadata = json::object({
    {"blobs", json::object({
      {"api", json::object({
        {"version", json::string("1.6")},  // Hard-coded in kubernetes.cc.
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
      })},
    })},
  });
  EXPECT_EQ(pod_metadata->ToString(), m[3].metadata().metadata->ToString());
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
        "KubernetesClusterLocation: TestClusterLocation\n"
        "KubernetesClusterName: TestClusterName\n"
        "KubernetesEndpointHost: " + server->GetUrl() + "\n"
        "KubernetesNodeName: TestNodeName\n"
        "MetadataIngestionRawContentVersion: \n"
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
                                     Timestamp last_timestamp) {
  for (int i = 0; i < 30; i++){
    const auto metadata_map = store.GetMetadataMap();
    const auto m = metadata_map.find(name);
    if (m != metadata_map.end() && m->second.collected_at > last_timestamp) {
      return true;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
  return false;
}

// Wait for updater's node watcher to connect to the server (hanging
// GET), then send 3 stream responses (2 adds and 1 delete) from the
// fake Kubernetes master and verify that the updater propagates them
// to the store.
void TestNodes(testing::FakeServer& server, MetadataStore& store,
               const std::string& nodes_watch_path) {
  const auto timeout = time::seconds(3);
  ASSERT_TRUE(server.WaitForMinTotalConnections(nodes_watch_path, 1, timeout));
  json::value node1 = json::object({
    {"metadata", json::object({
      {"name", json::string("TestNodeName1")},
      {"creationTimestamp", json::string("2018-03-03T01:23:45.678901234Z")},
    })},
  });
  json::value node2 = json::object({
    {"metadata", json::object({
      {"name", json::string("TestNodeName2")},
      {"creationTimestamp", json::string("2018-03-03T01:23:45.678901234Z")},
    })},
  });
  MonitoredResource resource1("k8s_node",
                              {{"cluster_name", "TestClusterName"},
                               {"location", "TestClusterLocation"},
                               {"node_name", "TestNodeName1"}});
  MonitoredResource resource2("k8s_node",
                              {{"cluster_name", "TestClusterName"},
                               {"location", "TestClusterLocation"},
                               {"node_name", "TestNodeName2"}});
  Timestamp last_timestamp = std::chrono::system_clock::now();

  // Add node #1 and wait until watcher has processed it (by polling the store).
  server.SendStreamResponse(nodes_watch_path, json::object({
    {"type", json::string("ADDED")},
    {"object", node1->Clone()},
  })->ToString());
  EXPECT_TRUE(WaitForNewerCollectionTimestamp(store, "", last_timestamp));

  // Verify node #1 values in the store & update last_timestamp.
  EXPECT_EQ(resource1, store.LookupResource("k8s_node.TestNodeName1"));
  {
    const auto metadata_map = store.GetMetadataMap();
    const auto& metadata = metadata_map.at("");
    EXPECT_EQ("", metadata.version);
    EXPECT_FALSE(metadata.is_deleted);
    last_timestamp = metadata.collected_at;
  }

  // Add node #2 and wait until watcher has processed it (by polling the store).
  server.SendStreamResponse(nodes_watch_path, json::object({
    {"type", json::string("ADDED")},
    {"object", node2->Clone()},
  })->ToString());
  EXPECT_TRUE(WaitForNewerCollectionTimestamp(store, "", last_timestamp));

  // Verify node #2 values in the store & update last_timestamp.
  EXPECT_EQ(resource2, store.LookupResource("k8s_node.TestNodeName2"));
  {
    const auto metadata_map = store.GetMetadataMap();
    const auto& metadata = metadata_map.at("");
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
  EXPECT_TRUE(WaitForNewerCollectionTimestamp(store, "", last_timestamp));

  // Verify node #1 values in the store.
  EXPECT_EQ(resource1, store.LookupResource("k8s_node.TestNodeName1"));
  {
    const auto metadata_map = store.GetMetadataMap();
    const auto& metadata = metadata_map.at("");
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
  json::value pod1 = json::object({
    {"metadata", json::object({
      {"name", json::string("TestPodName1")},
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
  json::value pod2 = json::object({
    {"metadata", json::object({
      {"name", json::string("TestPodName2")},
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
     {"pod_name", "TestPodName1"}});
  MonitoredResource k8s_pod_resource2(
    "k8s_pod",
    {{"cluster_name", "TestClusterName"},
     {"location", "TestClusterLocation"},
     {"namespace_name", "TestNamespace"},
     {"pod_name", "TestPodName2"}});
  MonitoredResource k8s_container_resource1(
    "k8s_container",
    {{"cluster_name", "TestClusterName"},
     {"container_name", "TestContainerName0"},
     {"location", "TestClusterLocation"},
     {"namespace_name", "TestNamespace"},
     {"pod_name", "TestPodName1"}});
  MonitoredResource k8s_container_resource2(
    "k8s_container",
    {{"cluster_name", "TestClusterName"},
     {"container_name", "TestContainerName0"},
     {"location", "TestClusterLocation"},
     {"namespace_name", "TestNamespace"},
     {"pod_name", "TestPodName2"}});
  Timestamp last_timestamp = std::chrono::system_clock::now();

  // Add pod #1 and wait until watch has processed it (by polling the store).
  server.SendStreamResponse(pods_watch_path, json::object({
    {"type", json::string("ADDED")},
    {"object", pod1->Clone()},
  })->ToString());
  EXPECT_TRUE(WaitForNewerCollectionTimestamp(
      store, "", last_timestamp));

  // Verify pod #1 values in the store & update last_timestamp.
  EXPECT_EQ(k8s_pod_resource1, store.LookupResource(
      "k8s_pod.TestNamespace.TestPodName1"));
  EXPECT_EQ(k8s_pod_resource1, store.LookupResource(
      "k8s_pod.TestPodUid1"));
  EXPECT_EQ(k8s_container_resource1, store.LookupResource(
      "k8s_container.TestNamespace.TestPodName1.TestContainerName0"));
  EXPECT_EQ(k8s_container_resource1, store.LookupResource(
      "k8s_container.TestPodUid1.TestContainerName0"));
  {
    const auto metadata_map = store.GetMetadataMap();

    const auto& k8s_pod_metadata = metadata_map.at("");
    EXPECT_FALSE(k8s_pod_metadata.ignore);
    EXPECT_EQ("", k8s_pod_metadata.version);
    EXPECT_FALSE(k8s_pod_metadata.is_deleted);

    const auto& k8s_container_metadata = metadata_map.at("");
    EXPECT_FALSE(k8s_container_metadata.ignore);
    EXPECT_EQ("",
              k8s_container_metadata.version);  // Hard-coded in kubernetes.cc.
    EXPECT_FALSE(k8s_container_metadata.is_deleted);

    last_timestamp = k8s_pod_metadata.collected_at;
  }

  // Add pod #2 and wait until watch has processed it (by polling the store).
  server.SendStreamResponse(pods_watch_path, json::object({
    {"type", json::string("ADDED")},
    {"object", pod2->Clone()},
  })->ToString());
  EXPECT_TRUE(WaitForNewerCollectionTimestamp(
      store, "", last_timestamp));

  // Verify pod #2 values in the store & update last_timestamp.
  EXPECT_EQ(k8s_pod_resource2, store.LookupResource(
      "k8s_pod.TestNamespace.TestPodName2"));
  EXPECT_EQ(k8s_pod_resource2, store.LookupResource(
      "k8s_pod.TestPodUid2"));
  EXPECT_EQ(k8s_container_resource2, store.LookupResource(
      "k8s_container.TestNamespace.TestPodName2.TestContainerName0"));
  EXPECT_EQ(k8s_container_resource2, store.LookupResource(
      "k8s_container.TestPodUid2.TestContainerName0"));
  {
    const auto metadata_map = store.GetMetadataMap();

    const auto& k8s_pod_metadata = metadata_map.at("");
    EXPECT_FALSE(k8s_pod_metadata.ignore);
    EXPECT_EQ("", k8s_pod_metadata.version);
    EXPECT_FALSE(k8s_pod_metadata.is_deleted);

    const auto& k8s_container_metadata = metadata_map.at("");
    EXPECT_FALSE(k8s_container_metadata.ignore);
    EXPECT_EQ("",
              k8s_container_metadata.version);  // Hard-coded in kubernetes.cc.
    EXPECT_FALSE(k8s_container_metadata.is_deleted);

    last_timestamp = k8s_pod_metadata.collected_at;
  }

  // Delete pod #1 and wait until watch has processed it (by polling the store).
  server.SendStreamResponse(pods_watch_path, json::object({
    {"type", json::string("DELETED")},
    {"object", pod1->Clone()},
  })->ToString());
  EXPECT_TRUE(WaitForNewerCollectionTimestamp(
      store, "", last_timestamp));

  // Verify pod #1 values in the store.
  EXPECT_EQ(k8s_pod_resource1, store.LookupResource(
      "k8s_pod.TestNamespace.TestPodName1"));
  EXPECT_EQ(k8s_pod_resource1, store.LookupResource(
      "k8s_pod.TestPodUid1"));
  EXPECT_EQ(k8s_container_resource1, store.LookupResource(
      "k8s_container.TestNamespace.TestPodName1.TestContainerName0"));
  EXPECT_EQ(k8s_container_resource1, store.LookupResource(
      "k8s_container.TestPodUid1.TestContainerName0"));
  {
    const auto metadata_map = store.GetMetadataMap();

    const auto& k8s_pod_metadata = metadata_map.at("");
    EXPECT_FALSE(k8s_pod_metadata.ignore);
    EXPECT_EQ("", k8s_pod_metadata.version);
    EXPECT_TRUE(k8s_pod_metadata.is_deleted);

    const auto& k8s_container_metadata = metadata_map.at("");
    EXPECT_FALSE(k8s_container_metadata.ignore);
    EXPECT_EQ("",
              k8s_container_metadata.version);  // Hard-coded in kubernetes.cc.
    EXPECT_TRUE(k8s_container_metadata.is_deleted);
  }
}

// Wait for updater's service & endpoints watchers to connect to the
// server (hanging GET), then send 6 stream responses (2 adds and 1
// delete each for services and endpoints) from the fake Kubernetes
// master and verify that the updater propagates them to the store.
void TestServicesAndEndpoints(testing::FakeServer& server, MetadataStore& store,
                              const std::string& services_watch_path,
                              const std::string& endpoints_watch_path) {
  const auto timeout = time::seconds(3);
  ASSERT_TRUE(
      server.WaitForMinTotalConnections(services_watch_path, 1, timeout));
  ASSERT_TRUE(
      server.WaitForMinTotalConnections(endpoints_watch_path, 1, timeout));
  json::value service1 = json::object({
    {"metadata", json::object({
      {"name", json::string("testname1")},
      {"namespace", json::string("testnamespace")},
    })},
  });
  json::value service2 = json::object({
    {"metadata", json::object({
      {"name", json::string("testname2")},
      {"namespace", json::string("testnamespace")},
    })},
  });
  json::value endpoint1 = json::object({
    {"metadata", json::object({
      {"name", json::string("testname1")},
      {"namespace", json::string("testnamespace")},
    })},
    {"subsets", json::array({
      json::object({
        {"addresses", json::array({
          json::object({
            {"targetRef", json::object({
              {"kind", json::string("Pod")},
              {"name", json::string("my-pod1")},
            })},
          }),
        })},
      }),
    })},
  });
  json::value endpoint2 = json::object({
    {"metadata", json::object({
      {"name", json::string("testname2")},
      {"namespace", json::string("testnamespace")},
    })},
    {"subsets", json::array({
      json::object({
        {"addresses", json::array({
          json::object({
            {"targetRef", json::object({
              {"kind", json::string("Pod")},
              {"name", json::string("my-pod2")},
            })},
          }),
        })},
      }),
    })},
  });
  MonitoredResource pod_mr1 = MonitoredResource("k8s_pod", {
    {"cluster_name", "TestClusterName"},
    {"namespace_name", "testnamespace"},
    {"pod_name", "my-pod1"},
    {"location", "TestClusterLocation"},
  });
  MonitoredResource pod_mr2 = MonitoredResource("k8s_pod", {
    {"cluster_name", "TestClusterName"},
    {"namespace_name", "testnamespace"},
    {"pod_name", "my-pod2"},
    {"location", "TestClusterLocation"},
  });
  MonitoredResource resource("k8s_cluster",
                             {{"cluster_name", "TestClusterName"},
                              {"location", "TestClusterLocation"}});
  Timestamp last_timestamp = std::chrono::system_clock::now();

  // Add service #1 and wait until watcher has processed it (by
  // polling the store).
  server.SendStreamResponse(services_watch_path, json::object({
    {"type", json::string("ADDED")},
    {"object", service1->Clone()},
  })->ToString());
  EXPECT_TRUE(WaitForNewerCollectionTimestamp(store, "", last_timestamp));

  // Verify service #1 (with no pod data) in the store & update
  // last_timestamp.
  {
    const auto metadata_map = store.GetMetadataMap();
    const auto& metadata = metadata_map.at("");
    EXPECT_EQ("", metadata.version);
    EXPECT_FALSE(metadata.is_deleted);
    EXPECT_EQ(Timestamp(), metadata.collected_at);
    json::value expected_cluster = json::object({
      {"blobs", json::object({
        {"services", json::array({
          json::object({
            {"api", json::object({
              {"version", json::string("")},  // Hard-coded in kubernetes.cc.
              {"pods", json::array({})},
              {"raw", service1->Clone()},
            })},
          }),
        })},
      })},
    });
    EXPECT_EQ(expected_cluster->ToString(), metadata.metadata->ToString());

    last_timestamp = metadata.collected_at;
  }

  // Add endpoint #1 and wait until watcher has processed it (by
  // polling the store).
  server.SendStreamResponse(endpoints_watch_path, json::object({
    {"type", json::string("ADDED")},
    {"object", endpoint1->Clone()},
  })->ToString());
  EXPECT_TRUE(WaitForNewerCollectionTimestamp(store, "", last_timestamp));

  // Verify that service #1 now has pods & update last_timestamp.
  {
    const auto metadata_map = store.GetMetadataMap();
    const auto& metadata = metadata_map.at("");
    EXPECT_EQ("", metadata.version);
    EXPECT_FALSE(metadata.is_deleted);
    EXPECT_EQ(Timestamp(), metadata.collected_at);
    json::value expected_cluster = json::object({
      {"blobs", json::object({
        {"services", json::array({
          json::object({
            {"api", json::object({
              {"version", json::string("")},  // Hard-coded in kubernetes.cc.
              {"pods", json::array({pod_mr1.ToJSON()})},
              {"raw", service1->Clone()},
            })},
          }),
        })},
      })},
    });
    EXPECT_EQ(expected_cluster->ToString(), metadata.metadata->ToString());

    last_timestamp = metadata.collected_at;
  }

  // Add endpoint #2 and wait until watcher has processed it (by
  // polling the store).
  server.SendStreamResponse(endpoints_watch_path, json::object({
    {"type", json::string("ADDED")},
    {"object", endpoint2->Clone()},
  })->ToString());
  EXPECT_TRUE(WaitForNewerCollectionTimestamp(store, "", last_timestamp));

  // Verify that store still only has service #1 & update last_timestamp.
  {
    const auto metadata_map = store.GetMetadataMap();
    const auto& metadata = metadata_map.at("");
    EXPECT_EQ("", metadata.version);
    EXPECT_FALSE(metadata.is_deleted);
    EXPECT_EQ(Timestamp(), metadata.collected_at);
    json::value expected_cluster = json::object({
      {"blobs", json::object({
        {"services", json::array({
          json::object({
            {"api", json::object({
              {"version", json::string("")},  // Hard-coded in kubernetes.cc.
              {"pods", json::array({pod_mr1.ToJSON()})},
              {"raw", service1->Clone()},
            })},
          }),
        })},
      })},
    });
    EXPECT_EQ(expected_cluster->ToString(), metadata.metadata->ToString());

    last_timestamp = metadata.collected_at;
  }

  // Add service #2 and wait until watcher has processed it (by
  // polling the store).
  server.SendStreamResponse(services_watch_path, json::object({
    {"type", json::string("ADDED")},
    {"object", service2->Clone()},
  })->ToString());
  EXPECT_TRUE(WaitForNewerCollectionTimestamp(store, "", last_timestamp));

  // Verify that store now has services #1 and #2 with pod data &
  // update last_timestamp.
  {
    const auto metadata_map = store.GetMetadataMap();
    const auto& metadata = metadata_map.at("");
    EXPECT_EQ("", metadata.version);
    EXPECT_FALSE(metadata.is_deleted);
    EXPECT_EQ(Timestamp(), metadata.collected_at);
    json::value expected_cluster = json::object({
      {"blobs", json::object({
        {"services", json::array({
          json::object({
            {"api", json::object({
              {"version", json::string("")},  // Hard-coded in kubernetes.cc.
              {"pods", json::array({pod_mr1.ToJSON()})},
              {"raw", service1->Clone()},
            })},
          }),
          json::object({
            {"api", json::object({
              {"version", json::string("")},  // Hard-coded in kubernetes.cc.
              {"pods", json::array({pod_mr2.ToJSON()})},
              {"raw", service2->Clone()},
            })},
          }),
        })},
      })},
    });
    EXPECT_EQ(expected_cluster->ToString(), metadata.metadata->ToString());

    last_timestamp = metadata.collected_at;
  }

  // Delete service #1 and wait until watcher has processed it (by
  // polling the store).
  server.SendStreamResponse(services_watch_path, json::object({
    {"type", json::string("DELETED")},
    {"object", service1->Clone()},
  })->ToString());
  EXPECT_TRUE(WaitForNewerCollectionTimestamp(store, "", last_timestamp));

  // Verify that store now has only service #2 & update last_timestamp.
  {
    const auto metadata_map = store.GetMetadataMap();
    const auto& metadata = metadata_map.at("");
    EXPECT_EQ("", metadata.version);
    EXPECT_FALSE(metadata.is_deleted);
    EXPECT_EQ(Timestamp(), metadata.collected_at);
    json::value expected_cluster = json::object({
      {"blobs", json::object({
        {"services", json::array({
          json::object({
            {"api", json::object({
              {"version", json::string("")},  // Hard-coded in kubernetes.cc.
              {"pods", json::array({pod_mr2.ToJSON()})},
              {"raw", service2->Clone()},
            })},
          }),
        })},
      })},
    });
    EXPECT_EQ(expected_cluster->ToString(), metadata.metadata->ToString());

    last_timestamp = metadata.collected_at;
  }

  // Delete endpoint #2 and wait until watcher has processed it (by
  // polling the store).
  server.SendStreamResponse(services_watch_path, json::object({
    {"type", json::string("DELETED")},
    {"object", endpoint2->Clone()},
  })->ToString());
  EXPECT_TRUE(WaitForNewerCollectionTimestamp(store, "", last_timestamp));

  // Verify that store now has no services.
  //
  // TODO: Is this the right behavior? We deleted endpoint #2 but not
  // service #2; probably the store should still have service #2 but
  // with no pod data.
  {
    const auto metadata_map = store.GetMetadataMap();
    const auto& metadata = metadata_map.at("");
    EXPECT_EQ("", metadata.version);
    EXPECT_FALSE(metadata.is_deleted);
    EXPECT_EQ(Timestamp(), metadata.collected_at);
    json::value expected_cluster = json::object({
      {"blobs", json::object({
        {"services", json::array({})},
      })},
    });
    EXPECT_EQ(expected_cluster->ToString(), metadata.metadata->ToString());
  }
}

}  // namespace

TEST_F(KubernetesTestFakeServerOneWatchRetryNodeLevelMetadata,
       KubernetesUpdater) {
  const std::string nodes_watch_path =
    "/api/v1/watch/nodes/TestNodeName?watch=true";
  const std::string pods_watch_path =
    "/api/v1/pods?fieldSelector=spec.nodeName%3DTestNodeName&watch=true";

  // Create a fake server representing the Kubernetes master.
  server->SetResponse("/api/v1/nodes?limit=1", "{}");
  server->SetResponse("/api/v1/pods?limit=1", "{}");
  server->AllowStream(nodes_watch_path);
  server->AllowStream(pods_watch_path);

  MetadataStore store(*config);
  KubernetesUpdater updater(*config, /*health_checker=*/nullptr, &store);
  updater.Start();

  // Test nodes & pods (but not services & endpoints).
  TestNodes(*server, store, nodes_watch_path);
  TestPods(*server, store, pods_watch_path);

  // Terminate the hanging GETs on the server so that the updater will finish.
  server->TerminateAllStreams();
}

TEST_F(KubernetesTestFakeServerThreeWatchRetriesNodeLevelMetadata,
       KubernetesUpdaterReconnection) {
  const std::string nodes_watch_path =
    "/api/v1/watch/nodes/TestNodeName?watch=true";
  const std::string pods_watch_path =
    "/api/v1/pods?fieldSelector=spec.nodeName%3DTestNodeName&watch=true";
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
    "/api/v1/watch/nodes/TestNodeName?watch=true";
  const std::string pods_watch_path =
    "/api/v1/pods?fieldSelector=spec.nodeName%3DTestNodeName&watch=true";
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
