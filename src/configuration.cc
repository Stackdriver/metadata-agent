#include "configuration.h"

#include <map>

#include <yaml-cpp/yaml.h>

namespace google {

namespace {

constexpr const char kDefaultProjectId[] = "";
constexpr const char kDefaultCredentialsFile[] = "";
constexpr const int kMetadataApiDefaultNumThreads = 3;
constexpr const int kMetadataApiDefaultPort = 8000;
constexpr const int kMetadataReporterDefaultIntervalSeconds = 60;
constexpr const int kDockerUpdaterDefaultIntervalSeconds = 60;
constexpr const char kMetadataIngestionDefaultEndpointFormat[] =
    "https://stackdriver.googleapis.com/v1beta2/projects/{{project_id}}"
    "/monitoredResourceMetadata:batchUpdate";
constexpr const char kDockerDefaultEndpointHost[] =
    "unix://%2Fvar%2Frun%2Fdocker.sock/";
constexpr const char kDockerDefaultApiVersion[] = "1.24";
constexpr const char kDockerDefaultContainerFilter[] = "limit=30";
constexpr const char kDefaultInstanceZone[] = "";

}

MetadataAgentConfiguration::MetadataAgentConfiguration()
    : project_id_(kDefaultProjectId),
      credentials_file_(kDefaultCredentialsFile),
      metadata_api_num_threads_(kMetadataApiDefaultNumThreads),
      metadata_api_port_(kMetadataApiDefaultPort),
      metadata_reporter_interval_seconds_(
          kMetadataReporterDefaultIntervalSeconds),
      metadata_ingestion_endpoint_format_(
          kMetadataIngestionDefaultEndpointFormat),
      docker_updater_interval_seconds_(kDockerUpdaterDefaultIntervalSeconds),
      docker_endpoint_host_(kDockerDefaultEndpointHost),
      docker_api_version_(kDockerDefaultApiVersion),
      docker_container_filter_(kDockerDefaultContainerFilter),
      instance_zone_(kDefaultInstanceZone) {}

MetadataAgentConfiguration::MetadataAgentConfiguration(
    const std::string& filename) : MetadataAgentConfiguration()
{
  std::lock_guard<std::mutex> lock(mutex_);
  if (filename.empty()) return;

  YAML::Node config = YAML::LoadFile(filename);
  project_id_ =
      config["ProjectId"].as<std::string>(kDefaultProjectId);
  credentials_file_ =
      config["CredentialsFile"].as<std::string>(kDefaultCredentialsFile);
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
  docker_updater_interval_seconds_ =
      config["DockerUpdaterIntervalSeconds"].as<int>(
          kDockerUpdaterDefaultIntervalSeconds);
  docker_endpoint_host_ =
      config["DockerEndpointHost"].as<std::string>(kDockerDefaultEndpointHost);
  docker_api_version_ =
      config["DockerApiVersion"].as<std::string>(
          kDockerDefaultApiVersion);
  docker_container_filter_ =
      config["DockerContainerFilter"].as<std::string>(
          kDockerDefaultContainerFilter);
  instance_zone_ =
      config["InstanceZone"].as<std::string>(kDefaultInstanceZone);
}

}  // google
