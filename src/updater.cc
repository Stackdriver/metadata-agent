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
          result.id, result.resource, std::move(result.metadata));
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

std::string GetMetadataString(const std::string& path) {
  http::client client;
  http::client::request request(
      "http://metadata.google.internal/computeMetadata/v1/" + path);
  request << boost::network::header("Metadata-Flavor", "Google");
  http::client::response response = client.get(request);
  return body(response);
}

std::string InstanceZone() {
  // Query the metadata server.
  // TODO: Other sources?
  static std::string zone("us-central1-a");
  if (zone.empty()) {
    zone = GetMetadataString("instance/zone");
  }
  return zone;
}

constexpr char docker_endpoint_host[] = "unix://%2Fvar%2Frun%2Fdocker.sock/";
constexpr char docker_endpoint_version[] = "v1.24";
constexpr char docker_endpoint_path[] = "/containers";

}

std::string NumericProjectId() {
  // Query the metadata server.
  // TODO: Other sources.
  static std::string project_id("1234567890");
  if (project_id.empty()) {
    project_id = GetMetadataString("project/numeric-project-id");
  }
  return project_id;
}

std::vector<PollingMetadataUpdater::ResourceMetadata> DockerMetadataQuery() {
  LOG(INFO) << "Docker Query called";
  const std::string project_id = NumericProjectId();
  const std::string zone = InstanceZone();
  const std::string docker_version(docker_endpoint_version);
  const std::string docker_endpoint(docker_endpoint_host +
                                    docker_version +
                                    docker_endpoint_path);
  http::local_client client;
  http::local_client::request list_request(docker_endpoint + "/json?all=true");
  http::local_client::response list_response = client.get(list_request);
  Timestamp collected_at = std::chrono::high_resolution_clock::now();
  LOG(ERROR) << "List response: " << body(list_response);
  json::value parsed_list = json::JSONParser::FromString(body(list_response));
  LOG(ERROR) << "Parsed list: " << *parsed_list;
  std::vector<PollingMetadataUpdater::ResourceMetadata> result;
  if (!parsed_list->Is<json::Array>()) {
    LOG(ERROR) << "List response is not an array!";
    return result;
  }
  const json::Array* container_list = parsed_list->As<json::Array>();
  for (const json::value& element : *container_list) {
    if (!element->Is<json::Object>()) {
      LOG(ERROR) << "Element " << *element << " is not an object!";
      continue;
    }
    const json::Object* container = element->As<json::Object>();
    auto id_it = container->find("Id");
    if (id_it == container->end()) {
      LOG(ERROR) << "There is no container id in " << *container;
      continue;
    }
    if (!id_it->second->Is<json::String>()) {
      LOG(ERROR) << "Container id " << *id_it->second << " is not a string";
      continue;
    }
    const std::string id = id_it->second->As<json::String>()->value();
    // Inspect the container.
    http::local_client::request inspect_request(docker_endpoint + "/" + id + "/json");
    http::local_client::response inspect_response = client.get(inspect_request);
    LOG(ERROR) << "Inspect response: " << body(inspect_response);
    json::value parsed_metadata =
        json::JSONParser::FromString(body(inspect_response));
    LOG(ERROR) << "Parsed metadata: " << *parsed_metadata;
    const MonitoredResource resource("docker_container", {
      {"project_id", project_id},
      {"location", zone},
      {"container_id", id},
    });

    if (!parsed_metadata->Is<json::Object>()) {
      LOG(ERROR) << "Metadata is not an object";
      continue;
    }
    const json::Object* container_desc = parsed_metadata->As<json::Object>();

    auto created_it = container_desc->find("Created");
    if (created_it == container_desc->end()) {
      LOG(ERROR) << "There is no created time in " << *container_desc;
      continue;
    }
    if (!created_it->second->Is<json::String>()) {
      LOG(ERROR) << "Created time " << *created_it->second << " is not a string";
      continue;
    }
    const std::string created_str = created_it->second->As<json::String>()->value();
    Timestamp created_at = rfc3339::FromString(created_str);

    auto state_it = container_desc->find("State");
    if (state_it == container_desc->end()) {
      LOG(ERROR) << "There is no state object in " << *container_desc;
      continue;
    }
    if (!state_it->second->Is<json::Object>()) {
      LOG(ERROR) << "State " << *state_it->second << " is not an object";
      continue;
    }
    const json::Object* state = state_it->second->As<json::Object>();
    auto dead_it = state->find("Dead");
    if (dead_it == state->end()) {
      LOG(ERROR) << "There is no dead indicator in " << *state;
      continue;
    }
    if (!dead_it->second->Is<json::Boolean>()) {
      LOG(ERROR) << "Dead indicator " << *dead_it->second << " is not a boolean";
      continue;
    }
    bool is_deleted = dead_it->second->As<json::Boolean>()->value();

    result.emplace_back("container/" + id, resource,
                        MetadataAgent::Metadata(docker_version, is_deleted,
                                                created_at, collected_at,
                                                std::move(parsed_metadata)));
  }
  return result;
}

}
