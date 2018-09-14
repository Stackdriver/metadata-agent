#include "../src/configuration.h"
#include "gtest/gtest.h"
#include "gmock/gmock.h"

namespace google {

void VerifyDefaultConfig(const Configuration& config) {
  EXPECT_EQ("", config.ProjectId());
  EXPECT_EQ("", config.CredentialsFile());
  EXPECT_EQ(3, config.MetadataApiNumThreads());
  EXPECT_EQ(8000, config.MetadataApiPort());
  EXPECT_EQ("0.0.0.0", config.MetadataApiBindAddress());
  EXPECT_EQ(".", config.MetadataApiResourceTypeSeparator());
  EXPECT_EQ(60, config.MetadataReporterIntervalSeconds());
  EXPECT_EQ(false, config.MetadataReporterPurgeDeleted());
  EXPECT_THAT(config.MetadataReporterUserAgent(),
              ::testing::StartsWith("metadata-agent/"));
  EXPECT_EQ("https://stackdriver.googleapis.com/"
            "v1beta2/projects/{{project_id}}/resourceMetadata:batchUpdate",
            config.MetadataIngestionEndpointFormat());
  EXPECT_EQ(8*1024*1024, config.MetadataIngestionRequestSizeLimitBytes());
  EXPECT_EQ(1000, config.MetadataIngestionRequestSizeLimitCount());
  EXPECT_EQ("0.1", config.MetadataIngestionRawContentVersion());
  EXPECT_EQ(60*60, config.InstanceUpdaterIntervalSeconds());
  EXPECT_EQ("", config.InstanceResourceType());
  EXPECT_EQ(0, config.DockerUpdaterIntervalSeconds());
  EXPECT_EQ("unix://%2Fvar%2Frun%2Fdocker.sock/", config.DockerEndpointHost());
  EXPECT_EQ("1.23", config.DockerApiVersion());
  EXPECT_EQ("limit=30", config.DockerContainerFilter());
  EXPECT_EQ(0, config.KubernetesUpdaterIntervalSeconds());
  EXPECT_EQ("https://kubernetes.default.svc", config.KubernetesEndpointHost());
  EXPECT_EQ("", config.KubernetesPodLabelSelector());
  EXPECT_EQ("", config.KubernetesClusterName());
  EXPECT_EQ("", config.KubernetesClusterLocation());
  EXPECT_EQ("", config.KubernetesNodeName());
  EXPECT_EQ(false, config.KubernetesUseWatch());
  EXPECT_EQ(false, config.KubernetesClusterLevelMetadata());
  EXPECT_EQ(true, config.KubernetesServiceMetadata());
  EXPECT_EQ("", config.InstanceId());
  EXPECT_EQ("", config.InstanceZone());
}

TEST(ConfigurationTest, NoConfig) {
  Configuration config;
  VerifyDefaultConfig(config);
}

TEST(ConfigurationTest, EmptyConfig) {
  Configuration config(std::istringstream(""));
  VerifyDefaultConfig(config);
}

TEST(ConfigurationTest, PopulatedConfig) {
  Configuration config(std::istringstream(
      "ProjectId: TestProjectId\n"
      "MetadataApiNumThreads: 13\n"
      "MetadataReporterPurgeDeleted: true\n"
      "MetadataReporterUserAgent: \"foobar/foobaz\"\n"
      "MetadataIngestionRequestSizeLimitCount: 500\n"
  ));
  EXPECT_EQ("TestProjectId", config.ProjectId());
  EXPECT_EQ(13, config.MetadataApiNumThreads());
  EXPECT_EQ(true, config.MetadataReporterPurgeDeleted());
  EXPECT_EQ("foobar/foobaz", config.MetadataReporterUserAgent());
  EXPECT_EQ(500, config.MetadataIngestionRequestSizeLimitCount());
}

TEST(ConfigurationTest, CommentSkipped) {
  Configuration config(std::istringstream(
      "ProjectId: TestProjectId\n"
      "#MetadataApiNumThreads: 13\n"
      "MetadataReporterPurgeDeleted: true\n"
  ));
  EXPECT_EQ(3, config.MetadataApiNumThreads());
}

TEST(ConfigurationTest, BlankLine) {
  Configuration config(std::istringstream(
      "ProjectId: TestProjectId\n"
      "\n"
      "\n"
      "MetadataReporterPurgeDeleted: true\n"
  ));
  EXPECT_EQ("TestProjectId", config.ProjectId());
  EXPECT_EQ(true, config.MetadataReporterPurgeDeleted());
}

class ConfigurationArgumentParserTest : public ::testing::Test {
 protected:
  static int ParseArguments(Configuration* config, int ac, char** av) {
    return config->ParseArguments(ac, av);
  }
};

TEST_F(ConfigurationArgumentParserTest, CommandLineOverride) {
  Configuration config(std::istringstream(
      "ProjectId: TestProjectId\n"
      "MetadataApiNumThreads: 13\n"
  ));
  // First, a sanity check.
  EXPECT_EQ("TestProjectId", config.ProjectId());
  EXPECT_EQ(13, config.MetadataApiNumThreads());
  EXPECT_EQ(false, config.MetadataReporterPurgeDeleted());

  char* arguments[] = {
    "/path/to/metadatad",
    "-o",
    "ProjectId=NewProjectId",
    "-o",
    "MetadataReporterPurgeDeleted=true",
  };
  ParseArguments(&config, sizeof(arguments) / sizeof(char*), arguments);

  EXPECT_EQ("NewProjectId", config.ProjectId());
  EXPECT_EQ(13, config.MetadataApiNumThreads());
  EXPECT_EQ(true, config.MetadataReporterPurgeDeleted());
}

}  // namespace google
