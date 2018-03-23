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

#include <boost/program_options.hpp>
#include <iostream>
#include <map>
#include <fstream>

#include <yaml-cpp/yaml.h>

#ifndef AGENT_VERSION
#define AGENT_VERSION 0.0
#endif

// https://gcc.gnu.org/onlinedocs/gcc-7.3.0/cpp/Stringizing.html
#define STRINGIFY_H(x) #x
#define STRINGIFY(x) STRINGIFY_H(x)

namespace google {

namespace {
constexpr const char kConfigFileFlag[] = "config-file";

constexpr const char kDefaultProjectId[] = "";
constexpr const char kDefaultCredentialsFile[] = "";
constexpr const int kMetadataApiDefaultNumThreads = 3;
constexpr const int kMetadataApiDefaultPort = 8000;
constexpr const char kMetadataApiDefaultResourceTypeSeparator[] = ".";
constexpr const int kMetadataReporterDefaultIntervalSeconds = 60;
constexpr const int kMetadataReporterDefaultPurgeDeleted = false;
constexpr const char kMetadataReporterDefaultUserAgent[] =
    "metadata-agent/" STRINGIFY(AGENT_VERSION);
constexpr const char kMetadataIngestionDefaultEndpointFormat[] =
    "https://stackdriver.googleapis.com/v1beta2/projects/{{project_id}}"
    "/resourceMetadata:batchUpdate";
constexpr const int kMetadataIngestionDefaultRequestSizeLimitBytes =
    8*1024*1024;
constexpr const char kMetadataIngestionDefaultRawContentVersion[] = "0.1";
constexpr const int kInstanceUpdaterDefaultIntervalSeconds = 60*60;
constexpr const char kDefaultInstanceResourceType[] =
    "";  // A blank value means "unspecified; detect via environment".
constexpr const int kDockerUpdaterDefaultIntervalSeconds = 60;
constexpr const char kDockerDefaultEndpointHost[] =
    "unix://%2Fvar%2Frun%2Fdocker.sock/";
constexpr const char kDockerDefaultApiVersion[] = "1.23";
constexpr const char kDockerDefaultContainerFilter[] = "limit=30";
constexpr const int kKubernetesUpdaterDefaultIntervalSeconds = 0;
constexpr const char kKubernetesDefaultEndpointHost[] =
    "https://kubernetes.default.svc";
constexpr const char kKubernetesDefaultPodLabelSelector[] = "";
constexpr const char kKubernetesDefaultClusterName[] = "";
constexpr const char kKubernetesDefaultClusterLocation[] = "";
constexpr const char kKubernetesDefaultNodeName[] = "";
constexpr const bool kKubernetesDefaultUseWatch = true;
constexpr const bool kKubernetesDefaultClusterLevelMetadata = false;
constexpr const char kDefaultInstanceId[] = "";
constexpr const char kDefaultInstanceZone[] = "";
constexpr const char kDefaultHealthCheckFile[] =
    "/var/run/metadata-agent/health/unhealthy";

}

MetadataAgentConfiguration::MetadataAgentConfiguration()
    : project_id_(kDefaultProjectId),
      credentials_file_(kDefaultCredentialsFile),
      verbose_logging_(false),
      metadata_api_num_threads_(kMetadataApiDefaultNumThreads),
      metadata_api_port_(kMetadataApiDefaultPort),
      metadata_api_resource_type_separator_(
          kMetadataApiDefaultResourceTypeSeparator),
      metadata_reporter_interval_seconds_(
          kMetadataReporterDefaultIntervalSeconds),
      metadata_reporter_purge_deleted_(
          kMetadataReporterDefaultPurgeDeleted),
      metadata_reporter_user_agent_(
          kMetadataReporterDefaultUserAgent),
      metadata_ingestion_endpoint_format_(
          kMetadataIngestionDefaultEndpointFormat),
      metadata_ingestion_request_size_limit_bytes_(
          kMetadataIngestionDefaultRequestSizeLimitBytes),
      metadata_ingestion_raw_content_version_(
          kMetadataIngestionDefaultRawContentVersion),
      instance_updater_interval_seconds_(
          kInstanceUpdaterDefaultIntervalSeconds),
      instance_resource_type_(kDefaultInstanceResourceType),
      docker_updater_interval_seconds_(kDockerUpdaterDefaultIntervalSeconds),
      docker_endpoint_host_(kDockerDefaultEndpointHost),
      docker_api_version_(kDockerDefaultApiVersion),
      docker_container_filter_(kDockerDefaultContainerFilter),
      kubernetes_updater_interval_seconds_(
          kKubernetesUpdaterDefaultIntervalSeconds),
      kubernetes_endpoint_host_(kKubernetesDefaultEndpointHost),
      kubernetes_pod_label_selector_(kKubernetesDefaultPodLabelSelector),
      kubernetes_cluster_name_(kKubernetesDefaultClusterName),
      kubernetes_cluster_location_(kKubernetesDefaultClusterLocation),
      kubernetes_node_name_(kKubernetesDefaultNodeName),
      kubernetes_use_watch_(kKubernetesDefaultUseWatch),
      kubernetes_cluster_level_metadata_(
          kKubernetesDefaultClusterLevelMetadata),
      instance_id_(kDefaultInstanceId),
      instance_zone_(kDefaultInstanceZone),
      health_check_file_(kDefaultHealthCheckFile) {}

int MetadataAgentConfiguration::ParseArguments(int ac, char** av) {
  std::string config_file;
  boost::program_options::options_description flags_desc;
  flags_desc.add_options()
      ("help,h", "Print help message")
      ("version,V", "Print the agent version")
      ("verbose,v", boost::program_options::bool_switch(&verbose_logging_),
           "Enable verbose logging")
      ;
  boost::program_options::options_description hidden_desc;
  hidden_desc.add_options()
      (kConfigFileFlag,
           boost::program_options::value<std::string>(&config_file)
               ->default_value(""),
           "Configuration file location")
      ;
  boost::program_options::options_description all_desc;
  all_desc.add(flags_desc).add(hidden_desc);
  boost::program_options::positional_options_description positional_desc;
  positional_desc.add(kConfigFileFlag, 1);
  boost::program_options::variables_map flags;
  try {
    boost::program_options::store(
        boost::program_options::command_line_parser(ac, av)
        .options(all_desc).positional(positional_desc).run(), flags);
    boost::program_options::notify(flags);

    if (flags.count("help")) {
      std::cout << flags_desc << std::endl;
      return -1;
    }
    if (flags.count("version")) {
      std::cout << "Stackdriver Metadata Agent v" << STRINGIFY(AGENT_VERSION)
                << std::endl;
      return -1;
    }

    ParseConfigurationFile(config_file);
    return 0;
  } catch (const boost::program_options::error& arg_error) {
    std::cerr << arg_error.what() << std::endl;
    std::cerr << flags_desc << std::endl;
    return 1;
  }
}

void MetadataAgentConfiguration::ParseConfigurationFile(const std::string& filename) {
  if (filename.empty()) return;

  std::ifstream input(filename);
  ParseConfigurationStream(input);
}

void MetadataAgentConfiguration::ParseConfigurationString(const std::string& input) {
  std::stringstream stream(input);
  ParseConfigurationStream(stream);
}

void MetadataAgentConfiguration::ParseConfigurationStream(std::istream& input) {
  YAML::Node config = YAML::Load(input);
  std::lock_guard<std::mutex> lock(mutex_);
  project_id_ =
      config["ProjectId"].as<std::string>(kDefaultProjectId);
  credentials_file_ =
      config["CredentialsFile"].as<std::string>(kDefaultCredentialsFile);
  metadata_api_num_threads_ =
      config["MetadataApiNumThreads"].as<int>(kMetadataApiDefaultNumThreads);
  metadata_api_port_ =
      config["MetadataApiPort"].as<int>(kMetadataApiDefaultPort);
  metadata_api_resource_type_separator_ =
      config["MetadataApiResourceTypeSeparator"].as<std::string>(
          kMetadataApiDefaultResourceTypeSeparator);
  metadata_reporter_interval_seconds_ =
      config["MetadataReporterIntervalSeconds"].as<int>(
          kMetadataReporterDefaultIntervalSeconds);
  metadata_reporter_purge_deleted_ =
      config["MetadataReporterPurgeDeleted"].as<bool>(
          kMetadataReporterDefaultPurgeDeleted);
  metadata_reporter_user_agent_ =
      config["MetadataReporterUserAgent"].as<std::string>(
          kMetadataReporterDefaultUserAgent);
  metadata_ingestion_endpoint_format_ =
      config["MetadataIngestionEndpointFormat"].as<std::string>(
          kMetadataIngestionDefaultEndpointFormat);
  metadata_ingestion_request_size_limit_bytes_ =
      config["MetadataIngestionRequestSizeLimitBytes"].as<int>(
          kMetadataIngestionDefaultRequestSizeLimitBytes);
  metadata_ingestion_raw_content_version_ =
      config["MetadataIngestionRawContentVersion"].as<std::string>(
          kMetadataIngestionDefaultRawContentVersion);
  instance_updater_interval_seconds_ =
      config["InstanceUpdaterIntervalSeconds"].as<int>(
          kInstanceUpdaterDefaultIntervalSeconds);
  instance_resource_type_ =
      config["InstanceResourceType"].as<std::string>(
          kDefaultInstanceResourceType);
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
  kubernetes_cluster_location_ =
      config["KubernetesClusterLocation"].as<std::string>(
          kKubernetesDefaultClusterLocation);
  kubernetes_node_name_ =
      config["KubernetesNodeName"].as<std::string>(kKubernetesDefaultNodeName);
  kubernetes_use_watch_ =
      config["KubernetesUseWatch"].as<bool>(kKubernetesDefaultUseWatch);
  kubernetes_cluster_level_metadata_ =
      config["KubernetesClusterLevelMetadata"].as<bool>(
          kKubernetesDefaultClusterLevelMetadata);
  instance_id_ =
      config["InstanceId"].as<std::string>(kDefaultInstanceId);
  instance_zone_ =
      config["InstanceZone"].as<std::string>(kDefaultInstanceZone);
  health_check_file_ =
      config["HealthCheckFile"].as<std::string>(kDefaultHealthCheckFile);
}

}  // google

#undef STRINGIFY
#undef STRINGIFY_H
