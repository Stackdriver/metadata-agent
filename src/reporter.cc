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
#include "http_common.h"
#include "json.h"
#include "logging.h"
#include "time.h"

namespace http = boost::network::http;

namespace google {

constexpr const char kMultipartBoundary[] = "publishMultipartPost";
constexpr const char kPublishPathFormat[] =
    "/v1beta3/projects/{{project_id}}/resourceMetadata:publish";
constexpr const char kGcpLocationFormat[] =
    "//cloud.google.com/locations/{{location_type}}/{{location}}";
constexpr const char kPartFormat[] =
    "--{{boundary}}\n"
    "Content-Type: application/http\n"
    "Content-Transfer-Encoding: binary\n"
    "Content-ID: {{name}}\n"
    "\n"
    "POST {{path}}\n"
    "Content-Type: application/json; charset=UTF-8\n"
    "ContentLength: {{length}}\n"
    "\n"
    "{{body}}\n";

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
      SendMetadata(store_->GetMetadata());
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
                         const std::string& endpoint,
                         const std::string& publish_path,
                         const std::string& auth_header,
                         const std::string& user_agent,
                         bool verbose_logging)
    throw (boost::system::system_error) {

  if (entries.size() == 1) {
    // Add a copy of the single entry, except for the `views` field. This allows
    // us to sent a request to the batch endpoint when we have a single request.
    // We're making use of the fact that duplicate metadata is ignored.
    const json::Object* single_request = entries[0]->As<json::Object>();
    json::value duplicate_request = json::object({  // ResourceMetadata
      {"name", single_request->at("name")->Clone()},
      {"type", single_request->at("type")->Clone()},
      {"location", single_request->at("location")->Clone()},
      {"state", single_request->at("state")->Clone()},
      {"evenTime", single_request->at("evenTime")->Clone()},
    });
    entries.emplace_back(std::move(duplicate_request));
  }
  const std::string content_type =
      std::string("multipart/mixed; boundary=") + kMultipartBoundary;
  http::client client;
  http::client::request request(endpoint);
  request << boost::network::header("User-Agent", user_agent);
  request << boost::network::header("Content-Type", content_type);
  request << boost::network::header("Authorization", auth_header);
  request << boost::network::header("Expect", "100-continue");
  std::ostringstream out;
  out << std::endl;

  for (json::value& entry : entries) {
    const json::Object* single_request = entry->As<json::Object>();
    const std::string request_body = single_request->ToString();
    out << format::Substitute(kPartFormat, {
      {"boundary", kMultipartBoundary},
      {"name", single_request->Get<json::String>("name")},
      {"path", publish_path},
      {"length", std::to_string(request_body.size())},
      {"body", request_body},
    });
  }
  out << "--" << kMultipartBoundary << "--" << std::endl;
  std::string multipart_body = out.str();
  const int total_length = multipart_body.size();

  request << boost::network::header("Content-Length",
                                    std::to_string(total_length));
  request << boost::network::body(multipart_body);

  if (verbose_logging) {
    LOG(INFO) << "About to send request: POST " << endpoint;
    LOG(INFO) << "Headers: " << request.headers();
    LOG(INFO) << "Body:" << std::endl << body(request);
  }

  http::client::response response = client.post(request);
  if (status(response) >= 300) {
    throw boost::system::system_error(
        boost::system::errc::make_error_code(
            boost::system::errc::not_connected),
        format::Substitute("Server responded with '{{message}}' ({{code}})",
                           {{"message", status_message(response)},
                            {"code", format::str(status(response))}}));
  }
  if (verbose_logging) {
    LOG(INFO) << format::Substitute(
        "Server responded with '{{message}}' ({{code}})",
        {{"message", status_message(response)},
         {"code", format::str(status(response))}});
    LOG(INFO) << "Headers: " << response.headers();
    LOG(INFO) << "Body:" << std::endl << body(response);
  }
  // TODO: process response.
}

}

std::string MetadataReporter::FullyQualifiedResourceLocation(
    const std::string& location) const {
  const std::string location_type =
      environment_.IsGcpLocationZonal(location) ? "zones": "regions";
  return format::Substitute(
      kGcpLocationFormat,
      {{"location_type", location_type}, {"location", location}});
}

void MetadataReporter::SendMetadata(
    std::vector<MetadataStore::Metadata>&& metadata)
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
  const std::string& endpoint = config_.MetadataIngestionEndpointFormat();
  const std::string& publish_path =
      format::Substitute(kPublishPathFormat, {{"project_id", project_id}});
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
  for (auto& m : metadata) {
    const std::string resource_location =
        FullyQualifiedResourceLocation(m.location);
    json::value metadata_entry =
        json::object({  // ResourceMetadata
          {"name", json::string(m.name)},
          {"type", json::string(m.type)},
          {"location", json::string(resource_location)},
          {"state", json::string(m.is_deleted ? "DELETED" : "EXISTS")},
          {"eventTime", json::string(time::rfc3339::ToString(m.collected_at))},
          {"views",
            json::object({
              {m.version,
                json::object({
                  {"schemaName", json::string(m.schema_name)},
                  {"stringContent", json::string(m.metadata->ToString())},
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
      SendMetadataRequest(std::move(entries), endpoint, publish_path,
                          auth_header, user_agent, config_.VerboseLogging());
      entries.clear();
      total_size = empty_size;
    }
    entries.emplace_back(std::move(metadata_entry));
    total_size += size;
  }
  if (!entries.empty()) {
    SendMetadataRequest(std::move(entries), endpoint, publish_path,
                        auth_header, user_agent, config_.VerboseLogging());
  }
}

}
