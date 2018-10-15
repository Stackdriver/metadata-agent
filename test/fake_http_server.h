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

#include "../src/time.h"

#include <boost/network/protocol/http/server.hpp>
#include <condition_variable>
#include <mutex>
#include <queue>

namespace google {
namespace testing {

using GetHandler =
    // (path, headers) -> body
    std::function<std::string(const std::string&,
                              std::map<std::string, std::string>&)>;
using PostHandler =
    // (path, headers, body) -> body
    std::function<std::string(const std::string&,
                              std::map<std::string, std::string>&,
                              std::string)>;

// Starts a server in a separate thread, allowing it to choose an
// available port.
class FakeServer {
 public:
  FakeServer();
  ~FakeServer();

  // A handler for GET requests that returns a constant string.
  static GetHandler Return(const std::string& body);

  // Returns the URL for this server without a trailing slash:
  // "http://<host>:<port>".
  std::string GetUrl();

  // Sets the response for GET requests to a path.
  void SetHandler(const std::string& path, GetHandler handler);

  // Sets the response for POST requests to a path.
  void SetHandler(const std::string& path, PostHandler handler);

  // Helper method for simple GET responses.
  void SetResponse(const std::string& path, const std::string& response) {
    SetHandler(path, Return(response));
  }

  // TODO: Consider changing the stream methods to operate on a stream
  // object, rather than looking up the path every time.

  // Indicates that for the given path, the server should stream
  // responses over a hanging GET.
  void AllowStream(const std::string& path);

  // Blocks until at least one client has connected to the given path.
  // Returns false if the timeout is reached with no client connections.
  bool WaitForOneStreamWatcher(const std::string& path, time::seconds timeout);

  // Sends a streaming response to all watchers for the given path.
  void SendStreamResponse(const std::string& path, const std::string& response);

  // Closes all open streams on the server.
  bool TerminateAllStreams(time::seconds timeout);

 private:
  struct Handler;
  typedef boost::network::http::server<Handler> Server;

  // Handler that maps paths to response strings.
  struct Handler {
    // Stream holds the state for an endpoint that streams data over a
    // hanging GET.
    //
    // There is a queue for each client, and the mutex guards access
    // to queues vector and any queue element.
    //
    // This struct only holds a pointer to each queue; it does not
    // take ownership.
    class Stream {
     public:
      void AddQueue(std::queue<std::string>* queue);
      void RemoveQueue(std::queue<std::string>* queue);
      bool WaitForOneWatcher(time::seconds timeout);
      bool WaitUntilNoWatchers(time::seconds timeout);
      void SendToAllQueues(const std::string& response);
      std::string GetNextResponse(std::queue<std::string>* queue);

     private:
      std::mutex mutex_;
      std::condition_variable cv_;
      // The vector elements are not owned by the Stream object.
      std::vector<std::queue<std::string>*> queues_;
    };

    void operator()(Server::request const &request,
                    Server::connection_ptr connection);

    std::map<std::string, GetHandler> get_handlers;
    std::map<std::string, PostHandler> post_handlers;
    std::map<std::string, Stream> path_streams;
  };

  Handler handler_;
  Server server_;
  std::thread server_thread_;
};

}  // testing
}  // google

#endif  // FAKE_HTTP_SERVER_H_
