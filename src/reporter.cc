/*
 * Copyright 2018 Google Inc.
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

#include "reporter.h"

#define BOOST_NETWORK_ENABLE_HTTPS
#include <boost/network/protocol/http/client.hpp>

#include "configuration.h"
#include "format.h"
#include "json.h"
#include "logging.h"
#include "time.h"

namespace http = boost::network::http;

namespace google {

constexpr const char kMultipartBoundary[] = "publishMultipartPost";
constexpr const char kBatchEndpoint[] = "/batch";
constexpr const char kMetadataIngestionPublishEndpoint[] =
    "/v1beta3/projects/{{project_id}}/resourceMetadata:publish";

MetadataReporter::MetadataReporter(const Configuration& config,
                                   MetadataStore* store, double period_s)
    : store_(store),
      config_(config),
      environment_(config_),
      auth_(environment_),
      period_(period_s),
      reporter_thread_([=]() { ReportMetadata(); }) {}

MetadataReporter::~MetadataReporter() {
  reporter_thread_.join();
}

void MetadataReporter::ReportMetadata() {
  LOG(INFO) << "Metadata reporter started";
  // Wait for the first collection to complete.
  // TODO: Come up with a more robust synchronization mechanism.
  std::this_thread::sleep_for(time::seconds(3));
  // TODO: Do we need to be able to stop this?
  while (true) {
    if (config_.VerboseLogging()) {
      LOG(INFO) << "Sending metadata request to server";
    }
    try {
      SendMetadata(store_->GetMetadataMap());
      if (config_.VerboseLogging()) {
        LOG(INFO) << "Metadata request sent successfully";
      }
      if (config_.MetadataReporterPurgeDeleted()) {
        store_->PurgeDeletedEntries();
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
                         const std::string& host,
                         const std::string& publish_endpoint,
                         const std::string& auth_header,
                         const std::string& user_agent,
                         bool verbose_logging)
    throw (boost::system::system_error) {

  const std::string batch_uri = host + kBatchEndpoint;
  const std::string content_type =
      std::string("multipart/mixed; boundary=") + kMultipartBoundary;
  http::client client;
  http::client::request request(batch_uri);
  request << boost::network::header("User-Agent", user_agent);
  request << boost::network::header("Content-Type", content_type);
  request << boost::network::header("Authorization", auth_header);
  std::ostringstream out;
  out << std::endl << "--" << kMultipartBoundary << std::endl;

  for (json::value& entry : entries) {
    const json::Object* single_request = entry->As<json::Object>();
    std::string request_body = single_request->ToString();
    out << "Content-Type: application/http" << std::endl;
    out << "Content-Transfer-Encoding: binary" << std::endl;
    out << "Content-ID: " << single_request->Get<json::String>("name")
        << std::endl;
    out << std::endl;
    out << "POST " << publish_endpoint << std::endl;
    out << "Content-Type: application/json; charset=UTF-8" << std::endl;
    out << "Content-Length: " << std::to_string(request_body.size())
        << std::endl;
    out << std::endl << request_body << std::endl;
    out << "--" << kMultipartBoundary << std::endl;
  }
  std::string multipart_body = out.str();
  request << boost::network::header(
      "Content-Length", std::to_string(multipart_body.size()));
  request << boost::network::body(multipart_body);

  if (verbose_logging) {
    LOG(INFO) << "About to send request: POST " << batch_uri
              << " User-Agent: " << user_agent
              << std::endl << body(request);
  }

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
    std::map<std::string, MetadataStore::Metadata>&& metadata)
    throw (boost::system::system_error) {
  if (metadata.empty()) {
    if (config_.VerboseLogging()) {
      LOG(INFO) << "No data to send";
    }
    return;
  }

  if (config_.VerboseLogging()) {
    LOG(INFO) << "Sending request to the server";
  }
  const std::string project_id = environment_.NumericProjectId();
  const std::string host = config_.MetadataIngestionHost();
  // The endpoint template is expected to be of the form
  // "/v1beta3/.../projects/{{project_id}}/...".
  const std::string endpoint =
      format::Substitute(kMetadataIngestionPublishEndpoint,
                         {{"project_id", project_id}});
  const std::string auth_header = auth_.GetAuthHeaderValue();
  const std::string user_agent = config_.MetadataReporterUserAgent();

  const json::value empty_request = json::object({
    {"entries", json::array({})},
  });
  const int empty_size = empty_request->ToString().size();

  const int limit_bytes = config_.MetadataIngestionRequestSizeLimitBytes();
  const int limit_count = config_.MetadataIngestionRequestSizeLimitCount();
  int total_size = empty_size;

  std::vector<json::value> entries;
  for (auto& entry : metadata) {
    const std::string& full_resource_name = entry.first;
    MetadataStore::Metadata& metadata = entry.second;
    json::value metadata_entry =
        json::object({
          {"name", json::string(full_resource_name)},
          {"type", json::string(metadata.type)},
          {"location", json::string(metadata.location)},
          {"state", json::string(metadata.is_deleted ? "DELETED" : "EXISTS")},
          {"createTime", json::string(
              time::rfc3339::ToString(metadata.created_at))},
          {"eventTime", json::string(
              time::rfc3339::ToString(metadata.collected_at))},
          {"views",
            json::object({
              {metadata.version,
                json::object({
                  {"schemaName", json::string(metadata.schema_name)},
                  {"stringContent", json::string(metadata.metadata->ToString())},
                })
              }
            })
          }
      });
    // TODO: This is probably all kinds of inefficient...
    const int size = metadata_entry->ToString().size();
    if (empty_size + size > limit_bytes) {
      LOG(ERROR) << "Individual entry too large: " << size
                 << "B is greater than the limit " << limit_bytes
                 << "B, dropping; entry " << *metadata_entry;
      continue;
    }
    if (entries.size() == limit_count || total_size + size > limit_bytes) {
      SendMetadataRequest(std::move(entries), host, endpoint, auth_header,
                          user_agent, config_.VerboseLogging());
      entries.clear();
      total_size = empty_size;
    }
    entries.emplace_back(std::move(metadata_entry));
    total_size += size;
  }
  if (!entries.empty()) {
    SendMetadataRequest(std::move(entries), host, endpoint, auth_header,
                        user_agent, config_.VerboseLogging());
  }
}

}
