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
constexpr const char kMetadataIngestionDefaultEndpointFormat[] =
    "https://stackdriver.googleapis.com/v1beta2/projects/{{project_id}}"
    "/resourceMetadata:batchUpdate";
constexpr const int kMetadataIngestionDefaultRequestSizeLimitBytes =
    8*1024*1024;
constexpr const int kDockerUpdaterDefaultIntervalSeconds = 60;
constexpr const char kDockerDefaultEndpointHost[] =
    "unix://%2Fvar%2Frun%2Fdocker.sock/";
constexpr const char kDockerDefaultApiVersion[] = "1.23";
constexpr const char kDockerDefaultContainerFilter[] = "limit=30";
constexpr const int kKubernetesUpdaterDefaultIntervalSeconds = 60;
constexpr const char kKubernetesDefaultEndpointHost[] = "https://kubernetes";
constexpr const char kKubernetesDefaultPodLabelSelector[] = "";
constexpr const char kKubernetesDefaultClusterName[] = "";
constexpr const char kDefaultInstanceId[] = "";
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
      metadata_ingestion_request_size_limit_bytes_(
          kMetadataIngestionDefaultRequestSizeLimitBytes),
      docker_updater_interval_seconds_(kDockerUpdaterDefaultIntervalSeconds),
      docker_endpoint_host_(kDockerDefaultEndpointHost),
      docker_api_version_(kDockerDefaultApiVersion),
      docker_container_filter_(kDockerDefaultContainerFilter),
      kubernetes_updater_interval_seconds_(
          kKubernetesUpdaterDefaultIntervalSeconds),
      kubernetes_endpoint_host_(kKubernetesDefaultEndpointHost),
      kubernetes_pod_label_selector_(kKubernetesDefaultPodLabelSelector),
      kubernetes_cluster_name_(kKubernetesDefaultClusterName),
      instance_id_(kDefaultInstanceId),
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
  metadata_ingestion_request_size_limit_bytes_ =
      config["MetadataIngestionRequestSizeLimitBytes"].as<int>(
          kMetadataIngestionDefaultRequestSizeLimitBytes);
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
  kubernetes_updater_interval_seconds_ =
      config["KubernetesUpdaterIntervalSeconds"].as<int>(
          kKubernetesUpdaterDefaultIntervalSeconds);
  kubernetes_endpoint_host_ =
      config["KubernetesEndpointHost"].as<std::string>(
          kKubernetesDefaultEndpointHost);
  kubernetes_pod_label_selector_ =
      config["KubernetesPodLabelSelector"].as<std::string>(
          kKubernetesDefaultPodLabelSelector);
  kubernetes_cluster_name_ =
      config["KubernetesClusterName"].as<std::string>(
          kKubernetesDefaultClusterName);
  instance_id_ =
      config["InstanceId"].as<std::string>(kDefaultInstanceId);
  instance_zone_ =
      config["InstanceZone"].as<std::string>(kDefaultInstanceZone);
}

}  // google
