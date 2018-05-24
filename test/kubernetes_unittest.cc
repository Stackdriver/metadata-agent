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
#include "fake_http_server.h"
#include "gtest/gtest.h"
#include "temp_file.h"

#include <boost/network/protocol/http/server.hpp>
#include <memory>

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
      json::value associations, Timestamp collected_at, bool is_deleted)
      throw(json::Exception) {
    return reader.GetPodMetadata(
        pod, std::move(associations), collected_at, is_deleted);
  }

  static MetadataUpdater::ResourceMetadata GetContainerMetadata(
      const KubernetesReader& reader, const json::Object* pod,
      const json::Object* container_spec, const json::Object* container_status,
      json::value associations, Timestamp collected_at, bool is_deleted)
      throw(json::Exception) {
    return reader.GetContainerMetadata(pod, container_spec, container_status,
                                       std::move(associations), collected_at,
                                       is_deleted);
  }

  static std::vector<MetadataUpdater::ResourceMetadata> GetPodAndContainerMetadata(
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

  static json::value ComputePodAssociations(const KubernetesReader& reader,
                                     const json::Object* pod) {
    return reader.ComputePodAssociations(pod);
  }

  static void UpdateOwnersCache(KubernetesReader* reader, const std::string& key,
                         const json::value& value) {
    reader->owners_[key] = value->Clone();
  }

  static MetadataUpdater::ResourceMetadata GetClusterMetadata(
      const KubernetesReader& reader, Timestamp collected_at)
      throw(json::Exception) {
    return reader.GetClusterMetadata(collected_at);
  }

  static const std::string& KubernetesApiToken(const KubernetesReader& reader) {
    return reader.KubernetesApiToken();
  }

  static const std::string& KubernetesNamespace(const KubernetesReader& reader) {
    return reader.KubernetesNamespace();
  }

  static void UpdateServiceToMetadataCache(
      KubernetesReader* reader, const json::Object* service, bool is_deleted)
      throw(json::Exception) {
    return reader->UpdateServiceToMetadataCache(service, is_deleted);
  }

  static void UpdateServiceToPodsCache(
      KubernetesReader* reader, const json::Object* endpoints, bool is_deleted)
      throw(json::Exception) {
    return reader->UpdateServiceToPodsCache(endpoints, is_deleted);
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
        "MetadataIngestionRawContentVersion: TestVersion\n"
      )));
  }

  std::unique_ptr<testing::FakeServer> metadata_server;
};

TEST_F(KubernetesTestNoInstance, GetNodeMetadata) {
  json::value node = json::object({
    {"metadata", json::object({
      {"name", json::string("testname")},
      {"creationTimestamp", json::string("2018-03-03T01:23:45.678901234Z")},
    })}
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
  EXPECT_EQ("TestVersion", m.metadata().version);
  EXPECT_FALSE(m.metadata().is_deleted);
  EXPECT_EQ(time::rfc3339::FromString("2018-03-03T01:23:45.678901234Z"),
            m.metadata().created_at);
  EXPECT_EQ(Timestamp(), m.metadata().collected_at);
  json::value big = json::object({
    {"blobs", json::object({
      {"api", json::object({
        {"version", json::string("1.6")},  // Hard-coded in kubernetes.cc.
        {"raw", std::move(node)},
      })},
    })},
  });
  EXPECT_EQ(big->ToString(), m.metadata().metadata->ToString());
}

TEST_F(KubernetesTestNoInstance, ComputePodAssociations) {
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
  UpdateOwnersCache(reader.get(), "1.2.3/TestKind/TestUID1", controller);
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
      {"nodeName", json::string("TestSpecNodeName")},
    })},
    {"version", json::string("TestVersion")},
  });
  const auto associations =
      ComputePodAssociations(*reader, pod->As<json::Object>());
  EXPECT_EQ(expected_associations->ToString(), associations->ToString());
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
  UpdateOwnersCache(reader.get(), "1.2.3/TestKind/TestUID1", controller);
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
  EXPECT_FALSE(m[0].metadata().ignore);
  EXPECT_EQ("1.6", m[0].metadata().version);
  EXPECT_FALSE(m[0].metadata().is_deleted);
  EXPECT_EQ(time::rfc3339::FromString("2018-03-03T01:23:45.678901234Z"),
            m[0].metadata().created_at);
  EXPECT_EQ(Timestamp(), m[0].metadata().collected_at);
  json::value container_metadata = json::object({
    {"blobs", json::object({
      {"association", json::object({
        {"raw", json::object({
          {"controllers", json::object({
            {"topLevelControllerName", json::string("TestPodName")},
            {"topLevelControllerType", json::string("Pod")},
          })},
          {"nodeName", json::string("TestSpecNodeName")},
        })},
        {"version", json::string("TestVersion")},
      })},
      {"spec", json::object({
        {"raw", json::object({
          {"name", json::string("TestContainerName0")},
        })},
        {"version", json::string("1.6")},
      })},
      {"status", json::object({
        {"raw", json::object({
          {"name", json::string("TestContainerName0")},
        })},
        {"version", json::string("1.6")},
      })},
    })},
  });
  EXPECT_EQ(container_metadata->ToString(), m[0].metadata().metadata->ToString());

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
  EXPECT_EQ("TestVersion", m[1].metadata().version);
  EXPECT_FALSE(m[1].metadata().is_deleted);
  EXPECT_EQ(time::rfc3339::FromString("2018-03-03T01:23:45.678901234Z"),
            m[1].metadata().created_at);
  EXPECT_EQ(Timestamp(), m[1].metadata().collected_at);
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
      {"association", json::object({
        {"raw", json::object({
          {"controllers", json::object({
            {"topLevelControllerName", json::string("TestPodName")},
            {"topLevelControllerType", json::string("Pod")},
          })},
          {"nodeName", json::string("TestSpecNodeName")},
        })},
        {"version", json::string("TestVersion")},
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
        "MetadataIngestionRawContentVersion: TestVersion\n"
      )));
  }
};

TEST_F(KubernetesTestWithInstance, GetNodeMetadata) {
  json::value node = json::object({
    {"metadata", json::object({
      {"name", json::string("testname")},
      {"creationTimestamp", json::string("2018-03-03T01:23:45.678901234Z")},
    })}
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

TEST_F(KubernetesTestWithInstance, ComputePodAssociations) {
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
  UpdateOwnersCache(reader.get(), "1.2.3/TestKind/TestUID1", controller);
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
      ComputePodAssociations(*reader, pod->As<json::Object>());
  EXPECT_EQ(expected_associations->ToString(), associations->ToString());
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

TEST_F(KubernetesTestWithInstance, GetClusterMetadataEmpty) {
  const auto m = GetClusterMetadata(*reader, Timestamp());
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

TEST_F(KubernetesTestWithInstance, GetClusterMetadataEmptyService) {
  json::value service = json::object({
    {"metadata", json::object({
      {"name", json::string("testname")},
      {"namespace", json::string("testnamespace")},
    })},
  });
  UpdateServiceToMetadataCache(
      reader.get(), service->As<json::Object>(), /*is_deleted=*/false);
  const auto m = GetClusterMetadata(*reader, Timestamp());
  EXPECT_TRUE(m.ids().empty());
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

TEST_F(KubernetesTestWithInstance, GetClusterMetadataServiceWithPods) {
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
  UpdateServiceToMetadataCache(
      reader.get(), service->As<json::Object>(), /*is_deleted=*/false);
  UpdateServiceToPodsCache(
      reader.get(), endpoints->As<json::Object>(), /*is_deleted=*/false);
  const auto m = GetClusterMetadata(*reader, Timestamp());
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

TEST_F(KubernetesTestWithInstance, GetClusterMetadataDeletedService) {
  json::value service = json::object({
    {"metadata", json::object({
      {"name", json::string("testname")},
      {"namespace", json::string("testnamespace")},
    })},
  });
  UpdateServiceToMetadataCache(
      reader.get(), service->As<json::Object>(), /*is_deleted=*/false);
  UpdateServiceToMetadataCache(
      reader.get(), service->As<json::Object>(), /*is_deleted=*/true);
  const auto m = GetClusterMetadata(*reader, Timestamp());
  EXPECT_TRUE(m.ids().empty());
  json::value empty_cluster = json::object({
    {"blobs", json::object({
      {"services", json::array({})},
    })},
  });
  EXPECT_EQ(empty_cluster->ToString(), m.metadata().metadata->ToString());
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
      json::string("TestAssociations"),
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
  EXPECT_EQ("1.6", m.metadata().version);
  EXPECT_FALSE(m.metadata().is_deleted);
  EXPECT_EQ(time::rfc3339::FromString("2018-03-03T01:23:45.678901234Z"),
            m.metadata().created_at);
  EXPECT_EQ(Timestamp(), m.metadata().collected_at);
  EXPECT_FALSE(m.metadata().ignore);
  json::value expected_metadata = json::object({
    {"blobs", json::object({
      {"association", json::string("TestAssociations")},
      {"labels", json::object({
        {"raw", json::object({
          {"label", json::string("TestLabel")},
        })},
        {"version", json::string("1.6")},
      })},
      {"spec", json::object({
        {"raw", json::object({
          {"name", json::string("TestSpecName")},
        })},
        {"version", json::string("1.6")},
      })},
      {"status", json::object({
        {"raw", json::object({
          {"containerID", json::string("docker://TestContainerID")},
        })},
        {"version", json::string("1.6")},
      })},
    })},
  });
  EXPECT_EQ(expected_metadata->ToString(), m.metadata().metadata->ToString());
}

TEST_F(KubernetesTestWithInstance, GetPodAndContainerMetadata) {
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
  UpdateOwnersCache(reader.get(), "1.2.3/TestKind/TestUID1", controller);
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
  EXPECT_FALSE(m[1].metadata().ignore);
  EXPECT_EQ("1.6", m[1].metadata().version);
  EXPECT_FALSE(m[1].metadata().is_deleted);
  EXPECT_EQ(time::rfc3339::FromString("2018-03-03T01:23:45.678901234Z"),
            m[1].metadata().created_at);
  EXPECT_EQ(Timestamp(), m[1].metadata().collected_at);
  json::value container_metadata = json::object({
    {"blobs", json::object({
      {"association", json::object({
        {"raw", json::object({
          {"controllers", json::object({
            {"topLevelControllerName", json::string("TestPodName")},
            {"topLevelControllerType", json::string("Pod")},
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
      })},
      {"spec", json::object({
        {"raw", json::object({
          {"name", json::string("TestContainerName0")},
        })},
        {"version", json::string("1.6")},
      })},
      {"status", json::object({
        {"raw", json::object({
          {"name", json::string("TestContainerName0")},
        })},
        {"version", json::string("1.6")},
      })},
    })},
  });
  EXPECT_EQ(container_metadata->ToString(), m[1].metadata().metadata->ToString());

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
      {"association", json::object({
        {"raw", json::object({
          {"controllers", json::object({
            {"topLevelControllerName", json::string("TestPodName")},
            {"topLevelControllerType", json::string("Pod")},
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
        "MetadataIngestionRawContentVersion: TestVersion\n"
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
  EXPECT_EQ("TestVersion", m[0].metadata().version);
  EXPECT_FALSE(m[0].metadata().is_deleted);
  EXPECT_EQ(time::rfc3339::FromString("2018-03-03T01:23:45.678901234Z"),
            m[0].metadata().created_at);
  json::value node_metadata = json::object({
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
  EXPECT_FALSE(m[2].metadata().ignore);
  EXPECT_EQ("1.6", m[2].metadata().version);
  EXPECT_FALSE(m[2].metadata().is_deleted);
  EXPECT_EQ(time::rfc3339::FromString("2018-03-03T01:23:45.678901234Z"),
            m[2].metadata().created_at);
  json::value container_metadata = json::object({
    {"blobs", json::object({
      {"association", json::object({
        {"raw", json::object({
          {"controllers", json::object({
            {"topLevelControllerName", json::string("TestPodName")},
            {"topLevelControllerType", json::string("Pod")},
          })},
          {"infrastructureResource", json::object({
            {"labels", json::object({
              {"instance_id", json::string("TestID")},
              {"zone", json::string("TestZone")},
            })},
            {"type", json::string("gce_instance")},
          })},
          {"nodeName", json::string("TestNodeName")},
        })},
        {"version", json::string("TestVersion")},
      })},
      {"spec", json::object({
        {"raw", json::object({
          {"name", json::string("TestContainerName0")},
        })},
        {"version", json::string("1.6")},
      })},
      {"status", json::object({
        {"raw", json::object({
          {"name", json::string("TestContainerName0")},
        })},
        {"version", json::string("1.6")},
      })},
    })},
  });
  EXPECT_EQ(container_metadata->ToString(), m[2].metadata().metadata->ToString());

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
      {"association", json::object({
        {"raw", json::object({
          {"controllers", json::object({
            {"topLevelControllerName", json::string("TestPodName")},
            {"topLevelControllerType", json::string("Pod")},
          })},
          {"infrastructureResource", json::object({
            {"labels", json::object({
              {"instance_id", json::string("TestID")},
              {"zone", json::string("TestZone")},
            })},
            {"type", json::string("gce_instance")},
          })},
          {"nodeName", json::string("TestNodeName")},
        })},
        {"version", json::string("TestVersion")},
      })},
    })},
  });
  EXPECT_EQ(pod_metadata->ToString(), m[3].metadata().metadata->ToString());
}

// Repeatedly calls f(), and returns true if f() returns true within 3
// seconds.
bool Poll(std::function<bool()> f) {
  for (int j = 0; j < 30; j++) {
    if (f()) {
      return true;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
  return false;
}

TEST_F(KubernetesTest, KubernetesUpdater) {
  // Create a fake server representing the Kubernetes master.
  testing::FakeServer server;
  server.SetResponse("/api/v1/nodes?limit=1", "{}");
  server.SetResponse("/api/v1/pods?limit=1", "{}");
  server.AllowStream(
      "/api/v1/pods?fieldSelector=spec.nodeName%3DTestNodeName&watch=true");
  server.AllowStream(
      "/api/v1/watch/nodes/TestNodeName?watch=true");

  // Start the updater with a config that points to the fake server.
  Configuration config(std::istringstream(
      "KubernetesEndpointHost: " + server.GetUrl() + "\n"
      "KubernetesNodeName: TestNodeName\n"
      "KubernetesUseWatch: true\n"
  ));
  MetadataStore store(config);
  KubernetesUpdater updater(config, /*health_checker=*/nullptr, &store);
  std::thread updater_thread([&updater] { updater.start(); });

  // Wait for updater's watchers to connect to the server (hanging GETs).
  server.WaitForOneStreamWatcher(
      "/api/v1/pods?fieldSelector=spec.nodeName%3DTestNodeName&watch=true");
  server.WaitForOneStreamWatcher(
      "/api/v1/watch/nodes/TestNodeName?watch=true");

  // For nodes, send stream responses from the fake Kubernetes
  // master and verify that the updater propagates them to the store.
  for (int i = 0; i < 3; i++) {
    // Create a timestamp to embed in response.  We will check that
    // this value gets propagated to the store.
    std::string timestamp =
      std::string("2018-03-03T01:23:45.67890123") + std::to_string(i) + "Z";
    json::value resp = json::object({
      {"type", json::string("ADDED")},
      {"object", json::object({
        {"metadata", json::object({
          {"name", json::string("TestNodeName")},
          {"creationTimestamp", json::string(timestamp)},
        })}
      })}
    });
    // Send a response to the watcher.
    server.SendStreamResponse(
        "/api/v1/watch/nodes/TestNodeName?watch=true",
        resp->ToString());
    // Poll the store to see the updated timstamp.
    EXPECT_TRUE(
      Poll([&store, timestamp]() -> bool {
        MonitoredResource resource("k8s_node",
                                   {{"cluster_name", ""},
                                    {"location", ""},
                                    {"node_name", "TestNodeName"}});
        if (store.GetMetadataMap().size() == 0) {
          return false;
        }
        const auto meta = store.GetMetadataMap().find(resource);
        if (meta == store.GetMetadataMap().end()) {
          return false;
        }
        return time::rfc3339::ToString(meta->second.created_at) == timestamp;
      }));
  }

  // For pods, do the same thing.
  for (int i = 0; i < 3; i++) {
    std::string timestamp =
      std::string("2018-03-03T01:23:45.67890123") + std::to_string(i) + "Z";
    json::value resp = json::object({
      {"type", json::string("ADDED")},
      {"object", json::object({
        {"metadata", json::object({
          {"name", json::string("TestPodName")},
          {"namespace", json::string("TestNamespace")},
          {"uid", json::string("TestPodUid")},
          {"creationTimestamp", json::string(timestamp)},
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
    server.SendStreamResponse(
        "/api/v1/pods?fieldSelector=spec.nodeName%3DTestNodeName&watch=true",
        resp->ToString());
    // Poll the store to see the updated timstamp.
    EXPECT_TRUE(
      Poll([&store, timestamp]() -> bool {
        MonitoredResource resource("k8s_pod",
                                   {{"cluster_name", ""},
                                    {"location", ""},
                                    {"namespace_name", "TestNamespace"},
                                    {"pod_name", "TestPodName"}});
        if (store.GetMetadataMap().size() == 0) {
          return false;
        }
        const auto meta = store.GetMetadataMap().find(resource);
        if (meta == store.GetMetadataMap().end()) {
          return false;
        }
        return time::rfc3339::ToString(meta->second.created_at) == timestamp;
      }));
  }

  // Terminate the hanging GETs on the server so that the updater will finish.
  server.TerminateAllStreams();
  updater.stop();
  updater_thread.join();
}

}  // namespace google
