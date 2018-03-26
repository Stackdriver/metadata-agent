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

#include "docker.h"

#include "local_stream_http.h"
#include <boost/algorithm/string/join.hpp>
#include <boost/network/protocol/http/client.hpp>
#include <chrono>

#include "configuration.h"
#include "format.h"
#include "instance.h"
#include "json.h"
#include "logging.h"
#include "resource.h"
#include "store.h"
#include "time.h"

namespace http = boost::network::http;

namespace google {

namespace {

#if 0
constexpr const char kDockerEndpointHost[] = "unix://%2Fvar%2Frun%2Fdocker.sock/";
constexpr const char kDockerApiVersion[] = "1.23";
#endif
constexpr const char kDockerEndpointPath[] = "/containers";
constexpr const char kDockerContainerResourcePrefix[] = "container";

}

DockerReader::DockerReader(const Configuration& config)
    : config_(config), environment_(config) {}

bool DockerReader::ValidateConfiguration() const {
  try {
    const std::string container_filter(
        config_.DockerContainerFilter().empty()
        ? "" : "&" + config_.DockerContainerFilter());

    // A limit may exist in the container_filter, however, the docker API only
    // uses the first limit provided in the query params.
    (void) QueryDocker(std::string(kDockerEndpointPath) + 
                       "/json?all=true&limit=1" + container_filter);

    return true;
  } catch (const QueryException& e) {
    // Already logged.
    return false;
  }
}

MetadataUpdater::ResourceMetadata DockerReader::GetContainerMetadata(
    const json::Object* container, Timestamp collected_at) const
    throw(json::Exception) {
  const std::string zone = environment_.InstanceZone();

  const std::string id = container->Get<json::String>("Id");
  // Inspect the container.
  try {
    json::value raw_container = QueryDocker(
        std::string(kDockerEndpointPath) + "/" + id + "/json");
    if (config_.VerboseLogging()) {
      LOG(INFO) << "Parsed metadata: " << *raw_container;
    }

    const json::Object* container_desc = raw_container->As<json::Object>();
    const std::string name = container_desc->Get<json::String>("Name");

    const std::string created_str =
        container_desc->Get<json::String>("Created");
    Timestamp created_at = time::rfc3339::FromString(created_str);

    const json::Object* state = container_desc->Get<json::Object>("State");
    bool is_deleted = state->Get<json::Boolean>("Dead");

    const MonitoredResource resource("docker_container", {
      {"location", zone},
      {"container_id", id},
    });

    json::value instance_resource =
        InstanceReader::InstanceResource(environment_).ToJSON();

    json::value raw_metadata = json::object({
      {"blobs", json::object({
        {"association", json::object({
          {"version", json::string(config_.MetadataIngestionRawContentVersion())},
          {"raw", json::object({
            {"infrastructureResource", std::move(instance_resource)},
          })},
        })},
        {"api", json::object({
          {"version", json::string(config_.DockerApiVersion())},
          {"raw", std::move(raw_container)},
        })},
      })},
    });
    if (config_.VerboseLogging()) {
      LOG(INFO) << "Raw docker metadata: " << *raw_metadata;
    }

    const std::string resource_id = boost::algorithm::join(
        std::vector<std::string>{kDockerContainerResourcePrefix, id},
        config_.MetadataApiResourceTypeSeparator());
    const std::string resource_name = boost::algorithm::join(
        // The container name reported by Docker will always have a leading '/'.
        std::vector<std::string>{kDockerContainerResourcePrefix, name.substr(1)},
        config_.MetadataApiResourceTypeSeparator());
    return MetadataUpdater::ResourceMetadata(
        std::vector<std::string>{resource_id, resource_name},
        resource,
#ifdef ENABLE_DOCKER_METADATA
        MetadataStore::Metadata(config_.MetadataIngestionRawContentVersion(),
                                is_deleted, created_at, collected_at,
                                std::move(raw_metadata))
#else
        MetadataStore::Metadata::IGNORED()
#endif
    );
  } catch (const QueryException& e) {
    throw json::Exception("Container " + id +
                          " disappeared before we could inspect it");
  }
}

std::vector<MetadataUpdater::ResourceMetadata>
    DockerReader::MetadataQuery() const {
  if (config_.VerboseLogging()) {
    LOG(INFO) << "Docker Query called";
  }
  const std::string zone = environment_.InstanceZone();
  const std::string docker_endpoint(config_.DockerEndpointHost() +
                                    "v" + config_.DockerApiVersion() +
                                    kDockerEndpointPath);
  const std::string container_filter(
      config_.DockerContainerFilter().empty()
      ? "" : "&" + config_.DockerContainerFilter());
  std::vector<MetadataUpdater::ResourceMetadata> result;
  try {
    json::value parsed_list = QueryDocker(
        std::string(kDockerEndpointPath) + "/json?all=true" + container_filter);
    Timestamp collected_at = std::chrono::system_clock::now();
    if (config_.VerboseLogging()) {
      LOG(INFO) << "Parsed list: " << *parsed_list;
    }
    const json::Array* container_list = parsed_list->As<json::Array>();
    for (const json::value& raw_container : *container_list) {
      try {
        const json::Object* container = raw_container->As<json::Object>();
        result.emplace_back(GetContainerMetadata(container, collected_at));
      } catch (const json::Exception& e) {
        LOG(ERROR) << e.what();
        continue;
      }
    }
  } catch (const json::Exception& e) {
    LOG(ERROR) << e.what();
  } catch (const QueryException& e) {
    // Already logged.
  }
  return result;
}

json::value DockerReader::QueryDocker(const std::string& path) const
    throw(QueryException, json::Exception) {
  const std::string endpoint(config_.DockerEndpointHost() +
                             "v" + config_.DockerApiVersion() +
                             path);
  http::local_client client;
  http::local_client::request request(endpoint);
  if (config_.VerboseLogging()) {
    LOG(INFO) << "QueryDocker: Contacting " << endpoint;
  }
  try {
    http::local_client::response response = client.get(request);
    if (status(response) >= 300) {
      throw boost::system::system_error(
          boost::system::errc::make_error_code(boost::system::errc::not_connected),
          format::Substitute("Server responded with '{{message}}' ({{code}})",
                             {{"message", status_message(response)},
                              {"code", format::str(status(response))}}));
    }
#ifdef VERBOSE
    LOG(DEBUG) << "QueryDocker: Response: " << body(response);
#endif
    return json::Parser::FromString(body(response));
  } catch (const boost::system::system_error& e) {
    LOG(ERROR) << "Failed to query " << endpoint << ": " << e.what();
    throw QueryException(endpoint + " -> " + e.what());
  }
}

bool DockerUpdater::ValidateConfiguration() const {
  if (!PollingMetadataUpdater::ValidateConfiguration()) {
    return false;
  }

  return reader_.ValidateConfiguration();
}

}
