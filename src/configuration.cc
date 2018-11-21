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

class Option {
 public:
  virtual ~Option() = default;
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
Option* option(T& var, const D& default_) {
  return new OptionDef<T, D>(var, default_);
}

}

class Configuration::OptionMap
    : public std::map<std::string, std::unique_ptr<Option>> {
 public:
  OptionMap(std::vector<std::pair<const char*, Option*>>&& v) {
    for (const auto& p : v) {
      emplace(p.first, std::unique_ptr<Option>(p.second));
    }
  }
};

Configuration::Configuration()
    : verbose_logging_(false),
      options_(new OptionMap({
        {"ProjectId", option(project_id_, "")},
        {"CredentialsFile", option(credentials_file_, "")},
        {"MetadataApiNumThreads", option(metadata_api_num_threads_, 3)},
        {"MetadataApiPort", option(metadata_api_port_, 8000)},
        {"MetadataApiBindAddress",
         option(metadata_api_bind_address_, "0.0.0.0")},
        {"MetadataApiResourceTypeSeparator",
         option(metadata_api_resource_type_separator_, ".")},
        {"MetadataReporterIntervalSeconds",
         option(metadata_reporter_interval_seconds_, 60)},
        {"MetadataReporterPurgeDeleted",
         option(metadata_reporter_purge_deleted_, false)},
        {"MetadataReporterUserAgent",
         option(metadata_reporter_user_agent_,
                "metadata-agent/" STRINGIFY(AGENT_VERSION))},
        {"MetadataIngestionEndpointFormat",
         option(metadata_ingestion_endpoint_format_,
                "https://stackdriver.googleapis.com/v1beta2/projects/"
                "{{project_id}}/resourceMetadata:batchUpdate")},
        {"MetadataIngestionRequestSizeLimitBytes",
         option(metadata_ingestion_request_size_limit_bytes_, 8*1024*1024)},
        {"MetadataIngestionRequestSizeLimitCount",
         option(metadata_ingestion_request_size_limit_count_, 1000)},
        {"MetadataIngestionRawContentVersion",
         option(metadata_ingestion_raw_content_version_, "0.1")},
        {"InstanceUpdaterIntervalSeconds",
         option(instance_updater_interval_seconds_, 60*60)},
        // A blank value means "unspecified; detect via environment".
        {"InstanceResourceType", option(instance_resource_type_, "")},
        {"DockerUpdaterIntervalSeconds",
         option(docker_updater_interval_seconds_, 0)},
        {"DockerEndpointHost",
         option(docker_endpoint_host_, "unix://%2Fvar%2Frun%2Fdocker.sock/")},
        {"DockerApiVersion", option(docker_api_version_, "1.23")},
        {"DockerContainerFilter", option(docker_container_filter_, "limit=30")},
        {"KubernetesUpdaterIntervalSeconds",
         option(kubernetes_updater_interval_seconds_, 0)},
        // Zero or a negative value means "infinite retries".
        {"KubernetesUpdaterWatchConnectionRetries",
         option(kubernetes_updater_watch_connection_retries_, 0)},
        {"KubernetesUpdaterWatchMaxConnectionFailures",
         option(kubernetes_updater_watch_max_connection_failures_, 15)},
        // Zero or a negative value means no reconnection.
        {"KubernetesUpdaterWatchReconnectIntervalSeconds",
         option(kubernetes_updater_watch_reconnect_interval_seconds_, 3600)},
        {"KubernetesEndpointHost",
         option(kubernetes_endpoint_host_, "https://kubernetes.default.svc")},
        {"KubernetesPodLabelSelector",
         option(kubernetes_pod_label_selector_, "")},
        {"KubernetesClusterName", option(kubernetes_cluster_name_, "")},
        {"KubernetesClusterLocation", option(kubernetes_cluster_location_, "")},
        {"KubernetesNodeName", option(kubernetes_node_name_, "")},
        {"KubernetesUseWatch", option(kubernetes_use_watch_, false)},
        {"KubernetesClusterLevelMetadata",
         option(kubernetes_cluster_level_metadata_, false)},
        {"KubernetesServiceMetadata",
         option(kubernetes_service_metadata_, true)},
        {"InstanceId", option(instance_id_, "")},
        {"InstanceZone", option(instance_zone_, "")},
        {"HealthCheckMaxDataAgeSeconds",
         option(health_check_max_data_age_seconds_, 5*60)},
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
