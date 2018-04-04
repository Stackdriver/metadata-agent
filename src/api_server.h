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
#ifndef API_SERVER_H_
#define API_SERVER_H_

#define BOOST_NETWORK_ENABLE_HTTPS
#include <boost/network/protocol/http/server.hpp>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace http = boost::network::http;

namespace google {

// Configuration object.
class Configuration;

// Storage for the metadata mapping.
class MetadataStore;

// A server that implements the metadata agent API.
class MetadataApiServer {
 public:
  MetadataApiServer(const Configuration& config, const MetadataStore& store,
                    int server_threads, const std::string& host, int port);
  ~MetadataApiServer();

 private:
  friend class ApiServerTest;

  class Dispatcher;
  using HttpServer = http::server<Dispatcher>;
  using Handler = std::function<void(const HttpServer::request&,
                                     std::shared_ptr<HttpServer::connection>)>;

  class Dispatcher {
   public:
    using HandlerMap = std::map<std::pair<std::string, std::string>, Handler>;
    Dispatcher(const HandlerMap& handlers, bool verbose);
    void operator()(const HttpServer::request& request,
                    std::shared_ptr<HttpServer::connection> conn);
    void log(const HttpServer::string_type& info);
   private:
    // A mapping from a method/prefix pair to the handler function.
    // Order matters: later entries override earlier ones.
    const HandlerMap handlers_;
    bool verbose_;
  };

  // Handler functions.
  void HandleMonitoredResource(const HttpServer::request& request,
                               std::shared_ptr<HttpServer::connection> conn);

  const Configuration& config_;
  const MetadataStore& store_;
  Dispatcher dispatcher_;
  HttpServer server_;
  std::vector<std::thread> server_pool_;
};

}

#endif  // API_SERVER_H_
