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

#include <boost/range/irange.hpp>
#include <prometheus/text_serializer.h>

#include "configuration.h"
#include "health_checker.h"
#include "http_common.h"
#include "logging.h"
#include "store.h"

namespace google {

MetadataApiServer::Dispatcher::Dispatcher(
    const HandlerMap& handlers, bool verbose)
    : handlers_(handlers), verbose_(verbose) {}

void MetadataApiServer::Dispatcher::operator()(
    const HttpServer::request& request,
    std::shared_ptr<HttpServer::connection> conn) const {
  if (verbose_) {
    LOG(INFO) << "Dispatcher called: " << request.method
              << " " << request.destination
              << " headers: " << request.headers
              << " body: " << request.body;
  }
  // Look for the longest match first. This means going backwards through
  // the map, since strings are sorted in lexicographical order.
  for (auto it = handlers_.crbegin(); it != handlers_.crend(); ++it) {
    const std::string& method = it->first.first;
    const std::string& prefix = it->first.second;
#ifdef VERBOSE
    LOG(DEBUG) << "Checking " << method << " " << prefix;
#endif
    if (request.method != method || request.destination.find(prefix) != 0) {
#ifdef VERBOSE
      LOG(DEBUG) << "No match; skipping " << method << " " << prefix;
#endif
      continue;
    }
#ifdef VERBOSE
    LOG(DEBUG) << "Handler found for " << request.method
               << " " << request.destination;
#endif
    const Handler& handler = it->second;
    handler(request, conn);
  }
}

void MetadataApiServer::Dispatcher::log(const HttpServer::string_type& info) const {
  LOG(ERROR) << info;
}


MetadataApiServer::MetadataApiServer(const Configuration& config,
                                     const HealthChecker* health_checker,
                                     const std::shared_ptr<prometheus::Collectable> collectable,
                                     const MetadataStore& store,
                                     int server_threads,
                                     const std::string& host, int port)
    : config_(config), health_checker_(health_checker), collectable_(collectable),
      store_(store),
      dispatcher_({
        {{"GET", "/monitoredResource/"},
         [=](const HttpServer::request& request,
             std::shared_ptr<HttpServer::connection> conn) {
             HandleMonitoredResource(request, conn);
         }},
        {{"GET", "/healthz"},
         [=](const HttpServer::request& request,
             std::shared_ptr<HttpServer::connection> conn) {
             HandleHealthz(request, conn);
         }},
        {{"GET", "/metrics"},
         [=](const HttpServer::request& request,
             std::shared_ptr<HttpServer::connection> conn) {
             HandleMetrics(request, conn);
         }},
      }, config_.VerboseLogging()),
      server_(
          HttpServer::options(dispatcher_)
              .address(host)
              .port(std::to_string(port))
              .reuse_address(true)),
      server_pool_()
{
  for (int i : boost::irange(0, server_threads)) {
    server_pool_.emplace_back([=]() { server_.run(); });
  }
}

void MetadataApiServer::Stop() {
  server_.stop();
  LOG(INFO) << "API server stopped";
}

MetadataApiServer::~MetadataApiServer() {
  Stop();
  for (auto& thread : server_pool_) {
    thread.join();
  }
}

void MetadataApiServer::HandleMonitoredResource(
    const HttpServer::request& request,
    std::shared_ptr<HttpServer::connection> conn) {
  // The format for the local metadata API request is:
  //   {host}:{port}/monitoredResource/{id}
  static const std::string kPrefix = "/monitoredResource/";;
  const std::string id = request.destination.substr(kPrefix.size());
  if (config_.VerboseLogging()) {
    LOG(INFO) << "Handler called for " << id;
  }
  try {
    const MonitoredResource& resource = store_.LookupResource(id);
    if (config_.VerboseLogging()) {
      LOG(INFO) << "Found resource for " << id << ": " << resource;
    }
    conn->set_status(HttpServer::connection::ok);

    std::string response = resource.ToJSON()->ToString();

    conn->set_headers(std::map<std::string, std::string>({
      {"Connection", "close"},
      {"Content-Length", std::to_string(response.size())},
      {"Content-Type", "application/json"},
    }));
    conn->write(response);
  } catch (const std::out_of_range& e) {
    // TODO: This could be considered log spam.
    // As we add more resource mappings, these will become less and less
    // frequent, and could be promoted to ERROR.
    if (config_.VerboseLogging()) {
      LOG(WARNING) << "No matching resource for " << id;
    }
    conn->set_status(HttpServer::connection::not_found);
    json::value json_response = json::object({
      {"status_code", json::number(404)},
      {"error", json::string("Not found")},
    });

    std::string response = json_response->ToString();

    conn->set_headers(std::map<std::string, std::string>({
      {"Connection", "close"},
      {"Content-Length", std::to_string(response.size())},
      {"Content-Type", "application/json"},
    }));
    conn->write(response);
  }
}

void MetadataApiServer::HandleHealthz(
    const HttpServer::request& request,
    std::shared_ptr<HttpServer::connection> conn) {
  std::set<std::string> unhealthy_components;
  if (health_checker_ != nullptr) {
    unhealthy_components = health_checker_->UnhealthyComponents();
  }
  if (unhealthy_components.empty()) {
    if (config_.VerboseLogging()) {
      LOG(INFO) << "/healthz returning 200";
    }
    conn->set_status(HttpServer::connection::ok);

    std::string response = "healthy";

    conn->set_headers(std::map<std::string, std::string>({
      {"Connection", "close"},
      {"Content-Length", std::to_string(response.size())},
      {"Content-Type", "text/plain"},
    }));
    conn->write(response);
  } else {
    LOG(WARNING) << "/healthz returning 500; unhealthy components: "
                 << boost::algorithm::join(unhealthy_components, ", ");
    conn->set_status(HttpServer::connection::internal_server_error);

    std::ostringstream response_stream("unhealthy components:\n");
    for (const auto& component : unhealthy_components) {
      response_stream << component << "\n";
    }

    std::string response = response_stream.str();

    conn->set_headers(std::map<std::string, std::string>({
      {"Connection", "close"},
      {"Content-Length", std::to_string(response.size())},
      {"Content-Type", "text/plain"},
    }));
    conn->write(response);
  }
}

void MetadataApiServer::HandleMetrics(
    const HttpServer::request& request,
    std::shared_ptr<HttpServer::connection> conn) {
  std::string response = SerializeMetricsToPrometheusTextFormat();
  conn->set_status(HttpServer::connection::ok);
  conn->set_headers(std::map<std::string, std::string>({
    {"Connection", "close"},
    {"Content-Length", std::to_string(response.size())},
    {"Content-Type", "application/json"},
  }));
  conn->write(response);
}

std::string MetadataApiServer::SerializeMetricsToPrometheusTextFormat() const {
  if (!collectable_) {
    return "";
  }

  prometheus::TextSerializer text_serializer;
  return std::move(text_serializer.Serialize(collectable_->Collect()));
}

}
