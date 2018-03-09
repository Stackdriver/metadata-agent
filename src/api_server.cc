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
#include "http_common.h"
#include "json.h"
#include "logging.h"
#include "oauth2.h"
#include "time.h"

namespace http = boost::network::http;

namespace google {

MetadataAgent::Metadata MetadataAgent::Metadata::IGNORED() {
  return MetadataAgent::Metadata();
}


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

  Handler handler_;
  HttpServer server_;
  std::vector<std::thread> server_pool_;
};

class MetadataReporter {
 public:
  MetadataReporter(MetadataAgent* agent, double period_s);
  ~MetadataReporter();

 private:
  using seconds = std::chrono::duration<double, std::chrono::seconds::period>;
  // Metadata reporter.
  void ReportMetadata();

  // Send the given set of metadata.
  void SendMetadata(
      std::map<MonitoredResource, MetadataAgent::Metadata>&& metadata)
      throw (boost::system::system_error);

  MetadataAgent* agent_;
  Environment environment_;
  OAuth2 auth_;
  // The reporting period in seconds.
  seconds period_;
  std::thread reporter_thread_;
};

MetadataApiServer::Handler::Handler(const MetadataAgent& agent)
    : agent_(agent) {}

void MetadataApiServer::Handler::operator()(const HttpServer::request& request,
                                            HttpServer::response& response) {
  static const std::string kPrefix = "/monitoredResource/";
  // The format for the local metadata API request is:
  //   {host}:{port}/monitoredResource/{id}
  if (agent_.config_.VerboseLogging()) {
    LOG(INFO) << "Handler called: " << request.method
              << " " << request.destination
              << " headers: " << request.headers
              << " body: " << request.body;
  }
  if (request.method == "GET" && request.destination.find(kPrefix) == 0) {
    std::string id = request.destination.substr(kPrefix.size());
    std::lock_guard<std::mutex> lock(agent_.resource_mu_);
    const auto result = agent_.resource_map_.find(id);
    if (result == agent_.resource_map_.end()) {
      // TODO: This could be considered log spam.
      // As we add more resource mappings, these will become less and less
      // frequent, and could be promoted to ERROR.
      if (agent_.config_.VerboseLogging()) {
        LOG(WARNING) << "No matching resource for " << id;
      }
      response = HttpServer::response::stock_reply(
          HttpServer::response::not_found, "");
    } else {
      const MonitoredResource& resource = result->second;
      if (agent_.config_.VerboseLogging()) {
        LOG(INFO) << "Found resource for " << id << ": " << resource;
      }
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
    server_pool_.emplace_back(&HttpServer::run, &server_);
  }
}

MetadataApiServer::~MetadataApiServer() {
  for (auto& thread : server_pool_) {
    thread.join();
  }
}

MetadataReporter::MetadataReporter(MetadataAgent* agent, double period_s)
    : agent_(agent),
      environment_(agent->config_),
      auth_(environment_),
      period_(period_s),
      reporter_thread_(&MetadataReporter::ReportMetadata, this) {}

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
    if (agent_->config_.VerboseLogging()) {
      LOG(INFO) << "Sending metadata request to server";
    }
    try {
      SendMetadata(agent_->GetMetadataMap());
      if (agent_->config_.VerboseLogging()) {
        LOG(INFO) << "Metadata request sent successfully";
      }
      if (agent_->config_.MetadataReporterPurgeDeleted()) {
        agent_->PurgeDeletedEntries();
      }
    } catch (const boost::system::system_error& e) {
      LOG(ERROR) << "Metadata request unsuccessful: " << e.what();
    }
    std::this_thread::sleep_for(period_);
  }
  //LOG(INFO) << "Metadata reporter exiting";
}

namespace {

void SendMetadataRequest(std::vector<json::value>&& entries,
                         const std::string& endpoint,
                         const std::string& auth_header,
                         bool verbose_logging)
    throw (boost::system::system_error) {
  json::value update_metadata_request = json::object({
    {"entries", json::array(std::move(entries))},
  });

  if (verbose_logging) {
    LOG(INFO) << "About to send request: POST " << endpoint
              << " " << *update_metadata_request;
  }

  http::client client;
  http::client::request request(endpoint);
  std::string request_body = update_metadata_request->ToString();
  request << boost::network::header("Content-Length",
                                    std::to_string(request_body.size()));
  request << boost::network::header("Content-Type", "application/json");
  request << boost::network::header("Authorization", auth_header);
  request << boost::network::body(request_body);
  http::client::response response = client.post(request);
  if (status(response) >= 300) {
    throw boost::system::system_error(
        boost::system::errc::make_error_code(boost::system::errc::not_connected),
        format::Substitute("Server responded with '{{message}}' ({{code}})",
                           {{"message", status_message(response)},
                            {"code", format::str(status(response))}}));
  }
  if (verbose_logging) {
    LOG(INFO) << "Server responded with " << body(response);
  }
  // TODO: process response.
}

}

void MetadataReporter::SendMetadata(
    std::map<MonitoredResource, MetadataAgent::Metadata>&& metadata)
    throw (boost::system::system_error) {
  if (metadata.empty()) {
    if (agent_->config_.VerboseLogging()) {
      LOG(INFO) << "No data to send";
    }
    return;
  }

  if (agent_->config_.VerboseLogging()) {
    LOG(INFO) << "Sending request to the server";
  }
  const std::string project_id = environment_.NumericProjectId();
  // The endpoint template is expected to be of the form
  // "https://stackdriver.googleapis.com/.../projects/{{project_id}}/...".
  const std::string endpoint =
      format::Substitute(agent_->config_.MetadataIngestionEndpointFormat(),
                         {{"project_id", project_id}});
  const std::string auth_header = auth_.GetAuthHeaderValue();

  const json::value empty_request = json::object({
    {"entries", json::array({})},
  });
  const int empty_size = empty_request->ToString().size();

  const int limit_bytes =
      agent_->config_.MetadataIngestionRequestSizeLimitBytes();
  int total_size = empty_size;

  std::vector<json::value> entries;
  for (auto& entry : metadata) {
    const MonitoredResource& resource = entry.first;
    MetadataAgent::Metadata& metadata = entry.second;
    if (metadata.ignore) {
      continue;
    }
    json::value metadata_entry =
        json::object({  // MonitoredResourceMetadata
          {"resource", resource.ToJSON()},
          {"rawContentVersion", json::string(metadata.version)},
          {"rawContent", std::move(metadata.metadata)},
          {"state", json::string(metadata.is_deleted ? "DELETED" : "ACTIVE")},
          {"createTime", json::string(time::rfc3339::ToString(metadata.created_at))},
          {"collectTime", json::string(time::rfc3339::ToString(metadata.collected_at))},
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
      SendMetadataRequest(std::move(entries), endpoint, auth_header,
                          agent_->config_.VerboseLogging());
      entries.clear();
      total_size = empty_size;
    }
    entries.emplace_back(std::move(metadata_entry));
    total_size += size;
  }
  if (!entries.empty()) {
    SendMetadataRequest(std::move(entries), endpoint, auth_header,
                        agent_->config_.VerboseLogging());
  }
}

MetadataAgent::MetadataAgent(const MetadataAgentConfiguration& config)
    : config_(config) {}

MetadataAgent::~MetadataAgent() {}

void MetadataAgent::UpdateResource(const std::vector<std::string>& resource_ids,
                                   const MonitoredResource& resource) {
  std::lock_guard<std::mutex> lock(resource_mu_);
  // TODO: How do we handle deleted resources?
  // TODO: Do we care if the value was already there?
  for (const std::string& id : resource_ids) {
    if (config_.VerboseLogging()) {
      LOG(INFO) << "Updating resource map '" << id << "'->" << resource;
    }
    resource_map_.emplace(id, resource);
  }
}

void MetadataAgent::UpdateMetadata(const MonitoredResource& resource,
                                   Metadata&& entry) {
  std::lock_guard<std::mutex> lock(metadata_mu_);
  if (config_.VerboseLogging()) {
    LOG(INFO) << "Updating metadata map " << resource << "->{"
              << "version: " << entry.version << ", "
              << "is_deleted: " << entry.is_deleted << ", "
              << "created_at: " << time::rfc3339::ToString(entry.created_at) << ", "
              << "collected_at: " << time::rfc3339::ToString(entry.collected_at)
              << ", "
              << "metadata: " << *entry.metadata << ", "
              << "ignore: " << entry.ignore
              << "}";
  }
  // Force value update. The repeated search is inefficient, but shouldn't
  // be a huge deal.
  metadata_map_.erase(resource);
  metadata_map_.emplace(resource, std::move(entry));
}

std::map<MonitoredResource, MetadataAgent::Metadata>
    MetadataAgent::GetMetadataMap() const {
  std::lock_guard<std::mutex> lock(metadata_mu_);

  std::map<MonitoredResource, Metadata> result;
  for (const auto& kv : metadata_map_) {
    const MonitoredResource& resource = kv.first;
    const Metadata& metadata = kv.second;
    result.emplace(resource, metadata.Clone());
  }
  return result;
}

void MetadataAgent::PurgeDeletedEntries() {
  std::lock_guard<std::mutex> lock(metadata_mu_);

  for (auto it = metadata_map_.begin(); it != metadata_map_.end(); ) {
    const MonitoredResource& resource = it->first;
    const Metadata& entry = it->second;
    if (entry.is_deleted) {
      if (config_.VerboseLogging()) {
        LOG(INFO) << "Purging metadata entry " << resource << "->{"
                  << "version: " << entry.version << ", "
                  << "is_deleted: " << entry.is_deleted << ", "
                  << "created_at: " << time::rfc3339::ToString(entry.created_at)
                  << ", "
                  << "collected_at: " << time::rfc3339::ToString(entry.collected_at)
                  << ", "
                  << "metadata: " << *entry.metadata << ", "
                  << "ignore: " << entry.ignore
                  << "}";
      }
      it = metadata_map_.erase(it);
    } else {
      ++it;
    }
  }
}

void MetadataAgent::start() {
  metadata_api_server_.reset(new MetadataApiServer(
      *this, config_.MetadataApiNumThreads(), "0.0.0.0",
      config_.MetadataApiPort()));
  reporter_.reset(new MetadataReporter(
      this, config_.MetadataReporterIntervalSeconds()));
}

}
