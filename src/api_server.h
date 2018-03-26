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
#include <memory>
#include <thread>
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
  MetadataApiServer(const Configuration& config,
                    const MetadataStore& store, int server_threads,
                    const std::string& host, int port);
  ~MetadataApiServer();

 private:
  class Handler;
  using HttpServer = http::server<Handler>;
  class Handler {
   public:
    Handler(const Configuration& config, const MetadataStore& store);
    void operator()(const HttpServer::request& request,
                    std::shared_ptr<HttpServer::connection> conn);
    void log(const HttpServer::string_type& info);
   private:
    const Configuration& config_;
    const MetadataStore& store_;
  };

  Handler handler_;
  HttpServer server_;
  std::vector<std::thread> server_pool_;
};

}

#endif  // API_SERVER_H_
