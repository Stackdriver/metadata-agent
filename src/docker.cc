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
#include <boost/network/protocol/http/client.hpp>
#include <chrono>

#include "json.h"
#include "logging.h"
#include "resource.h"
#include "time.h"

namespace http = boost::network::http;

namespace google {

namespace {

#if 0
constexpr const char kDockerEndpointHost[] = "unix://%2Fvar%2Frun%2Fdocker.sock/";
constexpr const char kDockerApiVersion[] = "1.23";
#endif
constexpr const char kDockerEndpointPath[] = "/containers";

}

DockerReader::DockerReader(const MetadataAgentConfiguration& config)
    : config_(config), environment_(config) {}

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
  http::local_client client;
  http::local_client::request list_request(
      docker_endpoint + "/json?all=true" + container_filter);
  std::vector<MetadataUpdater::ResourceMetadata> result;
  try {
    http::local_client::response list_response = client.get(list_request);
    Timestamp collected_at = std::chrono::system_clock::now();
    if (config_.VerboseLogging()) {
      LOG(INFO) << "List response: " << body(list_response);
    }
    json::value parsed_list = json::Parser::FromString(body(list_response));
    if (config_.VerboseLogging()) {
      LOG(INFO) << "Parsed list: " << *parsed_list;
    }
    const json::Array* container_list = parsed_list->As<json::Array>();
    for (const json::value& element : *container_list) {
      try {
        const json::Object* container = element->As<json::Object>();
        const std::string id = container->Get<json::String>("Id");
        // Inspect the container.
        http::local_client::request inspect_request(docker_endpoint + "/" + id + "/json");
        http::local_client::response inspect_response = client.get(inspect_request);
        if (config_.VerboseLogging()) {
          LOG(INFO) << "Inspect response: " << body(inspect_response);
        }
        json::value raw_docker =
            json::Parser::FromString(body(inspect_response));
        if (config_.VerboseLogging()) {
          LOG(INFO) << "Parsed metadata: " << *raw_docker;
        }

        const MonitoredResource resource("docker_container", {
          {"location", zone},
          {"container_id", id},
        });

        const json::Object* container_desc = raw_docker->As<json::Object>();
        const std::string name = container_desc->Get<json::String>("Name");

        const std::string created_str =
            container_desc->Get<json::String>("Created");
        Timestamp created_at = rfc3339::FromString(created_str);

        const json::Object* state = container_desc->Get<json::Object>("State");
        bool is_deleted = state->Get<json::Boolean>("Dead");

        const std::string resource_id =
            std::string("container") + config_.MetadataApiResourceTypeSeparator() + id;
        // The container name reported by Docker will always have a leading '/'.
        const std::string resource_name =
            std::string("container") + config_.MetadataApiResourceTypeSeparator() + name.substr(1);
        result.emplace_back(std::vector<std::string>{resource_id, resource_name},
                            resource,
                            MetadataAgent::Metadata(config_.DockerApiVersion(),
                                                    is_deleted, created_at, collected_at,
                                                    std::move(raw_docker)));
      } catch (const json::Exception& e) {
        LOG(ERROR) << e.what();
        continue;
      }
    }
  } catch (const json::Exception& e) {
    LOG(ERROR) << e.what();
  } catch (const boost::system::system_error& e) {
    LOG(ERROR) << "Failed to communicate with " << docker_endpoint << ": " << e.what();
  }
  return result;
}

}
