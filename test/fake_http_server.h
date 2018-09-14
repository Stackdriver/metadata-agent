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
#ifndef FAKE_HTTP_SERVER_H_
#define FAKE_HTTP_SERVER_H_

#include <boost/network/protocol/http/server.hpp>

namespace google {
namespace testing {

// Starts a server in a separate thread, allowing it to choose an
// available port.
class FakeServer {
 public:
  FakeServer();
  ~FakeServer();

  std::string GetUrl();
  void SetResponse(const std::string& path, const std::string& response);

 private:
  struct Handler;
  typedef boost::network::http::server<Handler> Server;

  // Handler that maps paths to response strings.
  struct Handler {
    void operator()(Server::request const &request,
                    Server::connection_ptr connection);
    std::map<std::string, std::string> path_responses;
  };

  Handler handler_;
  Server server_;
  std::thread server_thread_;
};

}  // testing
}  // google

#endif  // FAKE_HTTP_SERVER_H_
