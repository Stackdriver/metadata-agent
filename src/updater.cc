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

#include "updater.h"

#include <boost/network/protocol/http/server.hpp>
#include <boost/network/protocol/http/client.hpp>
#include <chrono>

#include "json.h"
#include "local_stream_http.h"
#include "logging.h"
#include "time.h"

namespace http = boost::network::http;

namespace google {

PollingMetadataUpdater::PollingMetadataUpdater(
    double period_s, MetadataAgent* store,
    std::function<std::vector<ResourceMetadata>()> query_metadata)
    : period_(period_s),
      store_(store),
      query_metadata_(query_metadata),
      timer_(),
      reporter_thread_() {}

PollingMetadataUpdater::~PollingMetadataUpdater() {
  reporter_thread_.join();
}

void PollingMetadataUpdater::start() {
  timer_.lock();
  LOG(INFO) << "Timer locked";
  reporter_thread_ =
      std::thread(std::bind(&PollingMetadataUpdater::PollForMetadata, this));
}

void PollingMetadataUpdater::stop() {
  timer_.unlock();
  LOG(INFO) << "Timer unlocked";
}

void PollingMetadataUpdater::PollForMetadata() {
  bool done = false;
  do {
    std::vector<ResourceMetadata> result_vector = query_metadata_();
    for (ResourceMetadata& result : result_vector) {
      store_->UpdateResource(
          result.ids, result.resource, std::move(result.metadata));
    }
    // An unlocked timer means we should stop updating.
    LOG(INFO) << "Trying to unlock the timer";
    auto start = std::chrono::high_resolution_clock::now();
    done = true;
    while (!timer_.try_lock_for(period_)) {
      auto now = std::chrono::high_resolution_clock::now();
      // Detect spurious wakeups.
      if (now - start >= period_) {
        LOG(INFO) << " Timer unlock timed out after "
                  << std::chrono::duration_cast<seconds>(now - start).count()
                  << "s (good)";
        start = now;
        done = false;
        break;
      };
    }
  } while (!done);
  LOG(INFO) << "Timer unlocked (stop polling)";
}

namespace {

#if 0
constexpr const char docker_endpoint_host[] = "unix://%2Fvar%2Frun%2Fdocker.sock/";
constexpr const char docker_api_version[] = "1.23";
#endif
constexpr const char docker_endpoint_path[] = "/containers";
constexpr const char resource_type_separator[] = ".";

}

DockerReader::DockerReader(const MetadataAgentConfiguration& config)
    : config_(config), environment_(config) {}

std::vector<PollingMetadataUpdater::ResourceMetadata>
    DockerReader::MetadataQuery() const {
  LOG(INFO) << "Docker Query called";
  const std::string zone = environment_.InstanceZone();
  const std::string docker_endpoint(config_.DockerEndpointHost() +
                                    "v" + config_.DockerApiVersion() +
                                    docker_endpoint_path);
  const std::string container_filter(
      config_.DockerContainerFilter().empty()
      ? "" : "&" + config_.DockerContainerFilter());
  http::local_client client;
  http::local_client::request list_request(
      docker_endpoint + "/json?all=true" + container_filter);
  http::local_client::response list_response = client.get(list_request);
  Timestamp collected_at = std::chrono::high_resolution_clock::now();
  LOG(ERROR) << "List response: " << body(list_response);
  json::value parsed_list = json::Parser::FromString(body(list_response));
  LOG(ERROR) << "Parsed list: " << *parsed_list;
  std::vector<PollingMetadataUpdater::ResourceMetadata> result;
  try {
    const json::Array* container_list = parsed_list->As<json::Array>();
    for (const json::value& element : *container_list) {
      try {
        const json::Object* container = element->As<json::Object>();
        const std::string id = container->Get<json::String>("Id");
        // Inspect the container.
        http::local_client::request inspect_request(docker_endpoint + "/" + id + "/json");
        http::local_client::response inspect_response = client.get(inspect_request);
        LOG(ERROR) << "Inspect response: " << body(inspect_response);
        json::value parsed_metadata =
            json::Parser::FromString(body(inspect_response));
        LOG(ERROR) << "Parsed metadata: " << *parsed_metadata;
        const MonitoredResource resource("docker_container", {
          {"location", zone},
          {"container_id", id},
        });

        const json::Object* container_desc = parsed_metadata->As<json::Object>();
        const std::string name = container_desc->Get<json::String>("Name");

        const std::string created_str =
            container_desc->Get<json::String>("Created");
        Timestamp created_at = rfc3339::FromString(created_str);

        const json::Object* state = container_desc->Get<json::Object>("State");
        bool is_deleted = state->Get<json::Boolean>("Dead");

        const std::string resource_id =
            std::string("container") + resource_type_separator + id;
        // The container name reported by Docker will always have a leading '/'.
        const std::string resource_name =
            std::string("containerName") + resource_type_separator + name.substr(1);
        result.emplace_back(std::vector<std::string>{resource_id, resource_name},
                            resource,
                            MetadataAgent::Metadata(config_.DockerApiVersion(),
                                                    is_deleted, created_at, collected_at,
                                                    std::move(parsed_metadata)));
      } catch (const json::Exception& e) {
        LOG(ERROR) << e.what();
        continue;
      }
    }
  } catch (const json::Exception& e) {
    LOG(ERROR) << e.what();
  }
  return result;
}

}
