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

#include "api_server.h"

#define BOOST_NETWORK_ENABLE_HTTPS
#include <boost/network/protocol/http/client.hpp>
#include <boost/network/protocol/http/server.hpp>
#include <boost/range/irange.hpp>
#include <ostream>
#include <thread>

#include "environment.h"
#include "format.h"
#include "json.h"
#include "logging.h"
#include "oauth2.h"
#include "time.h"

namespace http = boost::network::http;

namespace google {

class MetadataApiServer {
 public:
  MetadataApiServer(const MetadataAgent& agent, int server_threads,
                    const std::string& host, int port);
  ~MetadataApiServer();

 private:
  class Handler;
  using HttpServer = http::server<Handler>;
  class Handler {
   public:
    Handler(const MetadataAgent& agent);
    void operator()(const HttpServer::request& request,
                    HttpServer::response& response);
    void log(const HttpServer::string_type& info);
   private:
    const MetadataAgent& agent_;
  };
  friend std::ostream& operator<<(
      std::ostream& o, const HttpServer::request::headers_container_type& hv);

  Handler handler_;
  HttpServer server_;
  std::vector<std::thread> server_pool_;
};

class MetadataReporter {
 public:
  MetadataReporter(const MetadataAgent& agent, double period_s);
  ~MetadataReporter();

 private:
  using seconds = std::chrono::duration<double, std::chrono::seconds::period>;
  // Metadata reporter.
  void ReportMetadata();

  // Send the given set of metadata.
  void SendMetadata(
      std::map<MonitoredResource, MetadataAgent::Metadata>&& metadata);

  const MetadataAgent& agent_;
  Environment environment_;
  OAuth2 auth_;
  // The reporting period in seconds.
  seconds period_;
  std::thread reporter_thread_;
};


// To allow logging headers. TODO: move to a common location.
std::ostream& operator<<(
    std::ostream& o,
    const MetadataApiServer::HttpServer::request::headers_container_type& hv) {
  o << "[";
  for (const auto& h : hv) {
    o << " " << h.name << ": " << h.value;
  }
  o << " ]";
  return o;
}


MetadataApiServer::Handler::Handler(const MetadataAgent& agent)
    : agent_(agent) {}

void MetadataApiServer::Handler::operator()(const HttpServer::request& request,
                                            HttpServer::response& response) {
  static const std::string kPrefix = "/monitoredResource/";
  // The format for the local metadata API request is:
  //   {host}:{port}/monitoredResource/{id}
  LOG(INFO) << "Handler called: " << request.method
            << " " << request.destination
            << " headers: " << request.headers
            << " body: " << request.body;
  if (request.method == "GET" && request.destination.find(kPrefix) == 0) {
    std::string id = request.destination.substr(kPrefix.size());
    const auto result = agent_.resource_map_.find(id);
    if (result == agent_.resource_map_.end()) {
      // TODO: This could be considered log spam.
      // As we add more resource mappings, these will become less and less
      // frequent, and could be promoted to ERROR.
      LOG(WARNING) << "No matching resource for " << id;
      response = HttpServer::response::stock_reply(
          HttpServer::response::not_found, "");
    } else {
      const MonitoredResource& resource = result->second;
      LOG(INFO) << "Found resource for " << id << ": " << resource;
      response = HttpServer::response::stock_reply(
          HttpServer::response::ok, resource.ToJSON()->ToString());
    }
  }
}

void MetadataApiServer::Handler::log(const HttpServer::string_type& info) {
  LOG(ERROR) << info;
}


MetadataApiServer::MetadataApiServer(const MetadataAgent& agent,
                                     int server_threads,
                                     const std::string& host, int port)
    : handler_(agent),
      server_(
          HttpServer::options(handler_)
              .address(host)
              .port(std::to_string(port))),
      server_pool_()
{
  for (int i : boost::irange(0, server_threads)) {
    server_pool_.emplace_back(std::bind(&HttpServer::run, &server_));
  }
}

MetadataApiServer::~MetadataApiServer() {
  for (auto& thread : server_pool_) {
    thread.join();
  }
}

MetadataReporter::MetadataReporter(const MetadataAgent& agent, double period_s)
    : agent_(agent),
      environment_(agent.config_),
      auth_(environment_),
      period_(period_s),
      reporter_thread_(std::bind(&MetadataReporter::ReportMetadata, this)) {}

MetadataReporter::~MetadataReporter() {
  reporter_thread_.join();
}

void MetadataReporter::ReportMetadata() {
  LOG(INFO) << "Metadata reporter started";
  // Wait for the first collection to complete.
  // TODO: Come up with a more robust synchronization mechanism.
  std::this_thread::sleep_for(std::chrono::seconds(3));
  // TODO: Do we need to be able to stop this?
  while (true) {
    LOG(INFO) << "Sending metadata request to server";
    SendMetadata(agent_.GetMetadataMap());
    LOG(INFO) << "Metadata request sent successfully";
    std::this_thread::sleep_for(period_);
  }
  //LOG(INFO) << "Metadata reporter exiting";
}

namespace {

void SendMetadataRequest(std::vector<json::value>&& entries,
                         const std::string& endpoint,
                         const std::string& auth_header) {
  json::value update_metadata_request = json::object({
    {"entries", json::array(std::move(entries))},
  });

  LOG(INFO) << "About to send request: " << *update_metadata_request;

  http::client client;
  http::client::request request(endpoint);
  std::string request_body = update_metadata_request->ToString();
  request << boost::network::header("Content-Length",
                                    std::to_string(request_body.size()));
  request << boost::network::header("Content-Type", "application/json");
  request << boost::network::header("Authorization", auth_header);
  request << boost::network::body(request_body);
  http::client::response response = client.post(request);
  LOG(INFO) << "Server responded with " << body(response);
  // TODO: process response.
}

}

void MetadataReporter::SendMetadata(
    std::map<MonitoredResource, MetadataAgent::Metadata>&& metadata) {
  if (metadata.empty()) {
    LOG(INFO) << "No data to send";
    return;
  }

  LOG(INFO) << "Sending request to the server";
  const std::string project_id = environment_.NumericProjectId();
  const std::string endpoint =
      format::Substitute(agent_.config_.MetadataIngestionEndpointFormat(),
                         {{"project_id", project_id}});
  const std::string auth_header = auth_.GetAuthHeaderValue();

  const json::value empty_request = json::object({
    {"entries", json::array({})},
  });
  const int empty_size = empty_request->ToString().size();

  const int limit_bytes =
      agent_.config_.MetadataIngestionRequestSizeLimitBytes();
  int total_size = empty_size;

  std::vector<json::value> entries;
  for (auto& entry : metadata) {
    const MonitoredResource& resource = entry.first;
    MetadataAgent::Metadata& metadata = entry.second;
    json::value metadata_entry =
        json::object({  // MonitoredResourceMetadata
          {"resource", resource.ToJSON()},
          {"rawContentVersion", json::string(metadata.version)},
          {"rawContent", std::move(metadata.metadata)},
          {"state", json::string(metadata.is_deleted ? "DELETED" : "ACTIVE")},
          {"createTime", json::string(rfc3339::ToString(metadata.created_at))},
          {"collectTime", json::string(rfc3339::ToString(metadata.collected_at))},
        });
    // TODO: This is probably all kinds of inefficient...
    const int size = metadata_entry->ToString().size();
    if (empty_size + size > limit_bytes) {
      LOG(ERROR) << "Individual entry too large: " << size
                 << "B is greater than the limit " << limit_bytes
                 << "B, dropping; entry " << *metadata_entry;
      continue;
    }
    if (total_size + size > limit_bytes) {
      SendMetadataRequest(std::move(entries), endpoint, auth_header);
      entries.clear();
      total_size = empty_size;
    }
    entries.emplace_back(std::move(metadata_entry));
    total_size += size;
  }
  if (!entries.empty()) {
    SendMetadataRequest(std::move(entries), endpoint, auth_header);
  }
}

MetadataAgent::MetadataAgent(const MetadataAgentConfiguration& config)
    : config_(config) {}

MetadataAgent::~MetadataAgent() {}

void MetadataAgent::UpdateResource(const std::vector<std::string>& resource_ids,
                                   const MonitoredResource& resource,
                                   Metadata&& entry) {
  std::lock_guard<std::mutex> lock(mu_);
  // TODO: How do we handle deleted resources?
  // TODO: Do we care if the value was already there?
  for (const std::string& id : resource_ids) {
    LOG(INFO) << "Updating resource map '" << id << "'->" << resource;
    resource_map_.emplace(id, resource);
  }
  LOG(INFO) << "Updating metadata map " << resource << "->{"
            << "version: " << entry.version << ", "
            << "is_deleted: " << entry.is_deleted << ", "
            << "created_at: " << rfc3339::ToString(entry.created_at) << ", "
            << "collected_at: " << rfc3339::ToString(entry.collected_at) << ", "
            << "metadata: " << *entry.metadata
            << "}";
  // Force value update. The repeated search is inefficient, but shouldn't
  // be a huge deal.
  metadata_map_.erase(resource);
  metadata_map_.emplace(resource, std::move(entry));
}

std::map<MonitoredResource, MetadataAgent::Metadata>
    MetadataAgent::GetMetadataMap() const {
  std::lock_guard<std::mutex> lock(mu_);

  std::map<MonitoredResource, Metadata> result;
  for (const auto& kv : metadata_map_) {
    const MonitoredResource& resource = kv.first;
    const Metadata& metadata = kv.second;
    result.emplace(resource, metadata.Clone());
  }
  return result;
}

void MetadataAgent::start() {
  metadata_api_server_.reset(new MetadataApiServer(
      *this, config_.MetadataApiNumThreads(), "0.0.0.0",
      config_.MetadataApiPort()));
  reporter_.reset(new MetadataReporter(
      *this, config_.MetadataReporterIntervalSeconds()));
}

}
