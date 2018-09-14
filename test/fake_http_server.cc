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

#include "fake_http_server.h"

namespace google {
namespace testing {

FakeServer::FakeServer()
    // Note: An empty port selects a random available port (this behavior
    // is not documented).
    : server_(Server::options(handler_).address("127.0.0.1").port("")) {
  server_.listen();
  server_thread_ = std::thread([this] { server_.run(); });
}

FakeServer::~FakeServer() {
  server_.stop();
  server_thread_.join();
}

std::string FakeServer::GetUrl() {
  network::uri_builder builder;
  builder.scheme("http").host(server_.address()).port(server_.port());
  return builder.uri().string();
}

void FakeServer::SetResponse(const std::string& path,
                             const std::string& response) {
  handler_.path_responses[path] = response;
}

void FakeServer::Handler::operator()(Server::request const &request,
                                     Server::connection_ptr connection) {
  auto it = path_responses.find(request.destination);
  if (it != path_responses.end()) {
    connection->set_status(Server::connection::ok);
    connection->set_headers(std::map<std::string, std::string>({
        {"Content-Type", "text/plain"},
    }));
    connection->write(it->second);
  } else {
    // Note: We have to set headers; otherwise, an exception is thrown.
    connection->set_status(Server::connection::not_found);
    connection->set_headers(std::map<std::string, std::string>());
  }
}

}  // testing
}  // google
