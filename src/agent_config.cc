#include "agent_config.h"

#include <map>

#include <yaml-cpp/yaml.h>

namespace google {

namespace {

constexpr const int kMetadataApiDefaultNumThreads = 3;
constexpr const int kMetadataApiDefaultPort = 8000;
constexpr const int kMetadataReporterDefaultIntervalSeconds = 60;
constexpr const int kDockerUpdaterDefaultIntervalSeconds = 60;
constexpr const char kDefaultCredentialsFile[] = "";
constexpr const char kMetadataIngestionDefaultEndpointFormat[] =
    "https://monitoring.googleapis.com/v3/projects/{{project_id}}"
    "/updateResourceMetadata";
constexpr const char kDockerDefaultEndpointHost[] =
    "unix://%2Fvar%2Frun%2Fdocker.sock/";
constexpr const char kDockerEndpointVersion[] = "v1.24";

}

MetadataAgentConfiguration::MetadataAgentConfiguration() :
    metadata_api_num_threads_(kMetadataApiDefaultNumThreads),
    metadata_api_port_(kMetadataApiDefaultPort),
    metadata_reporter_interval_seconds_(
        kMetadataReporterDefaultIntervalSeconds),
    metadata_ingestion_endpoint_format_(
        kMetadataIngestionDefaultEndpointFormat),
    credentials_file_(kDefaultCredentialsFile),
    docker_updater_interval_seconds_(kDockerUpdaterDefaultIntervalSeconds),
    docker_endpoint_host_(kDockerDefaultEndpointHost),
    docker_endpoint_version_(kDockerEndpointVersion) {}

MetadataAgentConfiguration::MetadataAgentConfiguration(
    const std::string& filename) : MetadataAgentConfiguration()
{
  if (filename.empty()) return;

  YAML::Node config = YAML::LoadFile(filename);
  metadata_api_num_threads_ =
      config["MetadataApiNumThreads"].as<int>(kMetadataApiDefaultNumThreads);
  metadata_api_port_ =
      config["MetadataApiPort"].as<int>(kMetadataApiDefaultPort);
  metadata_reporter_interval_seconds_ =
      config["MetadataReporterIntervalSeconds"].as<int>(
          kMetadataReporterDefaultIntervalSeconds);
  metadata_ingestion_endpoint_format_ =
      config["MetadataIngestionEndpointFormat"].as<std::string>(
          kMetadataIngestionDefaultEndpointFormat);
  credentials_file_ =
      config["CredentialsFile"].as<std::string>(kDefaultCredentialsFile);
  docker_updater_interval_seconds_ =
      config["DockerUpdaterIntervalSeconds"].as<int>(
          kDockerUpdaterDefaultIntervalSeconds);
  docker_endpoint_host_ =
      config["DockerEndpointHost"].as<std::string>(kDockerDefaultEndpointHost);
  docker_endpoint_version_ =
      config["DockerEndpointVersion"].as<std::string>(kDockerEndpointVersion);
}

}  // google
