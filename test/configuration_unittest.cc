#include "../src/configuration.h"
#include "gtest/gtest.h"

namespace google {

class MetadataAgentConfigurationTest : public ::testing::Test {
 public:
  void CallParseConfiguration(MetadataAgentConfiguration& config, std::istream& stream) {
    config.ParseConfiguration(stream);
  }

  static void SetUpTestCase() {}
  static void TearDownTestCase() {}

  void TestDefaultConfig(MetadataAgentConfiguration& config) {
    EXPECT_EQ("", config.ProjectId());
    EXPECT_EQ("", config.CredentialsFile());
    EXPECT_EQ(3, config.MetadataApiNumThreads());
    EXPECT_EQ(8000, config.MetadataApiPort());
    EXPECT_EQ(".", config.MetadataApiResourceTypeSeparator());
    EXPECT_EQ(60, config.MetadataReporterIntervalSeconds());
    EXPECT_EQ(false, config.MetadataReporterPurgeDeleted());
    EXPECT_EQ(
        "https://stackdriver.googleapis.com/"
        "v1beta2/projects/{{project_id}}/resourceMetadata:batchUpdate",
        config.MetadataIngestionEndpointFormat());
    EXPECT_EQ(8*1024*1024, config.MetadataIngestionRequestSizeLimitBytes());
    EXPECT_EQ("0.1", config.MetadataIngestionRawContentVersion());
    EXPECT_EQ(60*60, config.InstanceUpdaterIntervalSeconds());
    EXPECT_EQ("", config.InstanceResourceType());
    EXPECT_EQ(60, config.DockerUpdaterIntervalSeconds());
    EXPECT_EQ("unix://%2Fvar%2Frun%2Fdocker.sock/", config.DockerEndpointHost());
    EXPECT_EQ("1.23", config.DockerApiVersion());
    EXPECT_EQ("limit=30", config.DockerContainerFilter());
    EXPECT_EQ(0, config.KubernetesUpdaterIntervalSeconds());
    EXPECT_EQ("https://kubernetes.default.svc", config.KubernetesEndpointHost());
    EXPECT_EQ("", config.KubernetesPodLabelSelector());
    EXPECT_EQ("", config.KubernetesClusterName());
    EXPECT_EQ("", config.KubernetesClusterLocation());
    EXPECT_EQ("", config.KubernetesNodeName());
    EXPECT_EQ(true, config.KubernetesUseWatch());
    EXPECT_EQ("", config.InstanceId());
    EXPECT_EQ("", config.InstanceZone());
  }
};

TEST_F(MetadataAgentConfigurationTest, NoConfigTest) {
  MetadataAgentConfiguration config;
  TestDefaultConfig(config);
}

TEST_F(MetadataAgentConfigurationTest, EmptyConfigTest) {
  MetadataAgentConfiguration config;
  std::stringstream stream("");
  CallParseConfiguration(config, stream);
  TestDefaultConfig(config);
}

TEST_F(MetadataAgentConfigurationTest, PopulatedConfigTest) {
  MetadataAgentConfiguration config;
  std::stringstream stream(
      "ProjectId: TestProjectId\n"
      "MetadataApiNumThreads: 13\n"
      "MetadataReporterPurgeDeleted: true");
  CallParseConfiguration(config, stream);
  EXPECT_EQ("TestProjectId", config.ProjectId());
  EXPECT_EQ(13, config.MetadataApiNumThreads());
  EXPECT_EQ(true, config.MetadataReporterPurgeDeleted());
}

TEST_F(MetadataAgentConfigurationTest, CommentSkippedTest) {
  MetadataAgentConfiguration config;
  std::stringstream stream(
      "ProjectId: TestProjectId\n"
      "#MetadataApiNumThreads: 13\n"
      "MetadataReporterPurgeDeleted: true");
  CallParseConfiguration(config, stream);
  EXPECT_EQ(3, config.MetadataApiNumThreads());
}

}  // google
