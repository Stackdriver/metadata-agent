/*
 * Copyright 2017 Google Inc.
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
#ifndef AGENT_CONFIG_H_
#define AGENT_CONFIG_H_

#include <mutex>
#include <string>

namespace google {

class MetadataAgentConfiguration {
 public:
  MetadataAgentConfiguration();
  MetadataAgentConfiguration(const std::string& filename);

  // Shared configuration.
  const std::string& ProjectId() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return project_id_;
  }
  const std::string& CredentialsFile() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return credentials_file_;
  }
  // Metadata API server configuration options.
  int MetadataApiNumThreads() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return metadata_api_num_threads_;
  }
  int MetadataApiPort() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return metadata_api_port_;
  }
  // Metadata reporter options.
  int MetadataReporterIntervalSeconds() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return metadata_reporter_interval_seconds_;
  }
  const std::string& MetadataIngestionEndpointFormat() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return metadata_ingestion_endpoint_format_;
  }
  int MetadataIngestionRequestSizeLimitBytes() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return metadata_ingestion_request_size_limit_bytes_;
  }
  // Docker metadata updater options.
  int DockerUpdaterIntervalSeconds() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return docker_updater_interval_seconds_;
  }
  const std::string& DockerEndpointHost() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return docker_endpoint_host_;
  }
  const std::string& DockerApiVersion() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return docker_api_version_;
  }
  const std::string& DockerContainerFilter() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return docker_container_filter_;
  }
  const std::string& InstanceZone() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return instance_zone_;
  }

 private:
  mutable std::mutex mutex_;
  std::string project_id_;
  std::string credentials_file_;
  int metadata_api_num_threads_;
  int metadata_api_port_;
  int metadata_reporter_interval_seconds_;
  std::string metadata_ingestion_endpoint_format_;
  int metadata_ingestion_request_size_limit_bytes_;
  int docker_updater_interval_seconds_;
  std::string docker_endpoint_host_;
  std::string docker_api_version_;
  std::string docker_container_filter_;
  std::string instance_zone_;
};

}

#endif  // AGENT_CONFIG_H_
