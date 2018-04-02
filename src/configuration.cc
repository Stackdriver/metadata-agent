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
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>

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
constexpr const char kMetadataApiDefaultBindAddress[] = "0.0.0.0";
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
constexpr const int kMetadataIngestionDefaultRequestSizeLimitCount = 1000;
constexpr const char kMetadataIngestionDefaultRawContentVersion[] = "0.1";
constexpr const int kInstanceUpdaterDefaultIntervalSeconds = 60*60;
constexpr const char kDefaultInstanceResourceType[] =
    "";  // A blank value means "unspecified; detect via environment".
constexpr const int kDockerUpdaterDefaultIntervalSeconds = 0;
constexpr const char kDockerDefaultEndpointHost[] =
    "unix://%2Fvar%2Frun%2Fdocker.sock/";
constexpr const char kDockerDefaultApiVersion[] = "1.23";
constexpr const char kDockerDefaultContainerFilter[] = "limit=30";
constexpr const int kKubernetesUpdaterDefaultIntervalSeconds = 0;
constexpr const int kKubernetesUpdaterDefaultWatchConnectionRetries = 15;
constexpr const char kKubernetesDefaultEndpointHost[] =
    "https://kubernetes.default.svc";
constexpr const char kKubernetesDefaultPodLabelSelector[] = "";
constexpr const char kKubernetesDefaultClusterName[] = "";
constexpr const char kKubernetesDefaultClusterLocation[] = "";
constexpr const char kKubernetesDefaultNodeName[] = "";
constexpr const bool kKubernetesDefaultUseWatch = false;
constexpr const bool kKubernetesDefaultClusterLevelMetadata = false;
constexpr const bool kKubernetesDefaultServiceMetadata = true;
constexpr const char kDefaultInstanceId[] = "";
constexpr const char kDefaultInstanceZone[] = "";
constexpr const char kDefaultHealthCheckFile[] =
    "/var/run/metadata-agent/health/unhealthy";
constexpr const int kDefaultHealthCheckMaxDataAgeSeconds = 5*60;

}

namespace {

class Option {
 public:
  virtual void Set(const YAML::Node& value) = 0;
};

template<class T, class D>
class OptionDef : public Option {
 public:
  OptionDef(T& var, const D& default_) : var_(var) {
    var = default_;
  }
  void Set(const YAML::Node& value) { var_ = value.as<T>(var_); }
 private:
  T& var_;
};

template<class T, class D>
std::pair<std::string, Option*> option(
    const std::string& name, T& var, const D& default_) {
  return {name, new OptionDef<T, D>(var, default_)};
}

}

class Configuration::OptionMap
    : public std::map<std::string, std::unique_ptr<Option>> {
 public:
  OptionMap(std::vector<std::pair<std::string, Option*>>&& v) {
    for (const auto& p : v) {
      emplace(p.first, p.second);
    }
  }
};

Configuration::Configuration()
    : verbose_logging_(false),
      options_(new OptionMap({
        option("ProjectId", project_id_, ""),
        option("CredentialsFile", credentials_file_, ""),
        option("MetadataApiNumThreads", metadata_api_num_threads_, 3),
        option("MetadataApiPort", metadata_api_port_, 8000),
        option("MetadataApiBindAddress", metadata_api_bind_address_, "0.0.0.0"),
        option("MetadataApiResourceTypeSeparator", metadata_api_resource_type_separator_, "."),
        option("MetadataReporterIntervalSeconds", metadata_reporter_interval_seconds_, 60),
        option("MetadataReporterPurgeDeleted", metadata_reporter_purge_deleted_, false),
        option("MetadataReporterUserAgent", metadata_reporter_user_agent_, "metadata-agent/" STRINGIFY(AGENT_VERSION)),
        option("MetadataIngestionEndpointFormat", metadata_ingestion_endpoint_format_, "https://stackdriver.googleapis.com/v1beta2/projects/{{project_id}}/resourceMetadata:batchUpdate"),
        option("MetadataIngestionRequestSizeLimitBytes", metadata_ingestion_request_size_limit_bytes_, 8*1024*1024),
        option("MetadataIngestionRequestSizeLimitCount", metadata_ingestion_request_size_limit_count_, 1000),
        option("MetadataIngestionRawContentVersion", metadata_ingestion_raw_content_version_, "0.1"),
        option("InstanceUpdaterIntervalSeconds", instance_updater_interval_seconds_, 60*60),
        // A blank value means "unspecified; detect via environment".
        option("InstanceResourceType", instance_resource_type_, ""),
        option("DockerUpdaterIntervalSeconds", docker_updater_interval_seconds_, 0),
        option("DockerEndpointHost", docker_endpoint_host_, "unix://%2Fvar%2Frun%2Fdocker.sock/"),
        option("DockerApiVersion", docker_api_version_, "1.23"),
        option("DockerContainerFilter", docker_container_filter_, "limit=30"),
        option("KubernetesUpdaterIntervalSeconds", kubernetes_updater_interval_seconds_, 0),
        option("KubernetesUpdaterWatchConnectionRetries", kubernetes_updater_watch_connection_retries_, 15),
        option("KubernetesEndpointHost", kubernetes_endpoint_host_, "https://kubernetes.default.svc"),
        option("KubernetesPodLabelSelector", kubernetes_pod_label_selector_, ""),
        option("KubernetesClusterName", kubernetes_cluster_name_, ""),
        option("KubernetesClusterLocation", kubernetes_cluster_location_, ""),
        option("KubernetesNodeName", kubernetes_node_name_, ""),
        option("KubernetesUseWatch", kubernetes_use_watch_, false),
        option("KubernetesClusterLevelMetadata", kubernetes_cluster_level_metadata_, false),
        option("KubernetesServiceMetadata", kubernetes_service_metadata_, true),
        option("InstanceId", instance_id_, ""),
        option("InstanceZone", instance_zone_, ""),
        option("HealthCheckFile", health_check_file_, "/var/run/metadata-agent/health/unhealthy"),
        option("HealthCheckMaxDataAgeSeconds", health_check_max_data_age_seconds_, 5*60),
      })) {}

Configuration::Configuration(std::istream& input) : Configuration() {
  ParseConfiguration(input);
}

Configuration::~Configuration() = default;

int Configuration::ParseArguments(int ac, char** av) {
  std::string config_file;
  boost::program_options::options_description flags_desc;
  flags_desc.add_options()
      ("help,h", "Print help message")
      ("version,V", "Print the agent version")
      ("verbose,v", boost::program_options::bool_switch(&verbose_logging_),
           "Enable verbose logging")
      ("option,o",
            boost::program_options::value<std::vector<std::string>>()
                ->composing(),
            "Explicit configuration option, e.g. "
            "-o CredentialsFile=/tmp/token.json "
            "(can be specified multiple times)")
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
    ParseConfigFile(config_file);

    // Command line options override the options provided in the config file.
    if (flags.count("option")) {
      std::stringstream option_stream;
      const std::vector<std::string> options =
          flags["option"].as<std::vector<std::string>>();
      for (const std::string& option : options) {
        std::size_t separator_pos = option.find("=");
        if (separator_pos == std::string::npos) {
          std::cerr << "Invalid option " << option;
          return 1;
        }
        const std::string key = option.substr(0, separator_pos);
        const std::string value =
            option.substr(separator_pos + 1, std::string::npos);
        option_stream << key << ": " << value << "\n";
      }

#ifdef VERBOSE
      std::cout << "Options:\n" << option_stream.str() << std::endl;
#endif
      ParseConfiguration(option_stream);
    }

    return 0;
  } catch (const boost::program_options::error& arg_error) {
    std::cerr << arg_error.what() << std::endl;
    std::cerr << flags_desc << std::endl;
    return 1;
  }
}

void Configuration::ParseConfigFile(const std::string& filename) {
  if (filename.empty()) return;

  std::ifstream input(filename);
  ParseConfiguration(input);
}

void Configuration::ParseConfiguration(std::istream& input) {
  YAML::Node config = YAML::Load(input);
  std::lock_guard<std::mutex> lock(mutex_);
  for (const auto& kv : config) {
    const std::string key = kv.first.as<std::string>();
    auto option_it = options_->find(key);
    if (option_it != options_->end()) {
      option_it->second->Set(kv.second);
    } else {
      std::cerr << "Invalid option " << key << std::endl;
    }
  }
}

}  // google

#undef STRINGIFY
#undef STRINGIFY_H
