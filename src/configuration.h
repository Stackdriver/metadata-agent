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
  int ParseArguments(int ac, char** av);

  // Shared configuration.
  const std::string& ProjectId() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return project_id_;
  }
  const std::string& CredentialsFile() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return credentials_file_;
  }
  bool VerboseLogging() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return verbose_logging_;
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
  const std::string& MetadataApiResourceTypeSeparator() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return metadata_api_resource_type_separator_;
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
  const std::string& MetadataIngestionRawContentVersion() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return metadata_ingestion_raw_content_version_;
  }
  // Instance metadata updater options.
  int InstanceUpdaterIntervalSeconds() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return instance_updater_interval_seconds_;
  }
  const std::string& InstanceResourceType() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return instance_resource_type_;
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
  // GKE metadata updater options.
  int KubernetesUpdaterIntervalSeconds() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return kubernetes_updater_interval_seconds_;
  }
  const std::string& KubernetesEndpointHost() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return kubernetes_endpoint_host_;
  }
  const std::string& KubernetesPodLabelSelector() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return kubernetes_pod_label_selector_;
  }
  const std::string& KubernetesClusterName() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return kubernetes_cluster_name_;
  }
  const std::string& KubernetesNodeName() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return kubernetes_node_name_;
  }
  bool KubernetesUseWatch() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return kubernetes_use_watch_;
  }
  // Common metadata updater options.
  const std::string& InstanceId() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return instance_id_;
  }
  const std::string& InstanceZone() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return instance_zone_;
  }

 private:
  void ParseConfigFile(const std::string& filename);

  mutable std::mutex mutex_;
  std::string project_id_;
  std::string credentials_file_;
  bool verbose_logging_;
  int metadata_api_num_threads_;
  int metadata_api_port_;
  std::string metadata_api_resource_type_separator_;
  int metadata_reporter_interval_seconds_;
  std::string metadata_ingestion_endpoint_format_;
  int metadata_ingestion_request_size_limit_bytes_;
  std::string metadata_ingestion_raw_content_version_;
  int instance_updater_interval_seconds_;
  std::string instance_resource_type_;
  int docker_updater_interval_seconds_;
  std::string docker_endpoint_host_;
  std::string docker_api_version_;
  std::string docker_container_filter_;
  int kubernetes_updater_interval_seconds_;
  std::string kubernetes_endpoint_host_;
  std::string kubernetes_pod_label_selector_;
  std::string kubernetes_cluster_name_;
  std::string kubernetes_node_name_;
  bool kubernetes_use_watch_;
  std::string instance_id_;
  std::string instance_zone_;
};

}

#endif  // AGENT_CONFIG_H_
