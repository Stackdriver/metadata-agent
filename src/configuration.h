#ifndef AGENT_CONFIG_H_
#define AGENT_CONFIG_H_

#include <string>

namespace google {

class MetadataAgentConfiguration {
 public:
  MetadataAgentConfiguration();
  MetadataAgentConfiguration(const std::string& filename);

  // Shared configuration.
  const std::string& ProjectId() const {
    return project_id_;
  }
  const std::string& CredentialsFile() const {
    return credentials_file_;
  }
  // Metadata API server configuration options.
  int MetadataApiNumThreads() const {
    return metadata_api_num_threads_;
  }
  int MetadataApiPort() const {
    return metadata_api_port_;
  }
  // Metadata reporter options.
  int MetadataReporterIntervalSeconds() const {
    return metadata_reporter_interval_seconds_;
  }
  const std::string& MetadataIngestionEndpointFormat() const {
    return metadata_ingestion_endpoint_format_;
  }
  // Docker metadata updater options.
  int DockerUpdaterIntervalSeconds() const {
    return docker_updater_interval_seconds_;
  }
  const std::string& DockerEndpointHost() const {
    return docker_endpoint_host_;
  }
  const std::string& DockerEndpointVersion() const {
    return docker_endpoint_version_;
  }
  const std::string& DockerContainerFilter() const {
    return docker_container_filter_;
  }
  const std::string& InstanceZone() const {
    return instance_zone_;
  }

 private:
  std::string project_id_;
  std::string credentials_file_;
  int metadata_api_num_threads_;
  int metadata_api_port_;
  int metadata_reporter_interval_seconds_;
  std::string metadata_ingestion_endpoint_format_;
  int docker_updater_interval_seconds_;
  std::string docker_endpoint_host_;
  std::string docker_endpoint_version_;
  std::string docker_container_filter_;
  std::string instance_zone_;
};

}

#endif  // AGENT_CONFIG_H_
