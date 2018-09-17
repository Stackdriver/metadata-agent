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
#include <queue>

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

  // Indicates that for the given path, the server should stream
  // responses over a hanging GET.
  void AllowStream(const std::string& path);

  // Blocks until at least one client has connected to the given path.
  // Returns false if the timeout is reached with no client connections.
  bool WaitForOneStreamWatcher(const std::string& path, time::seconds timeout);

  // Sends a streaming response to all watchers for the given path.
  void SendStreamResponse(const std::string& path, const std::string& response);

  // Closes all open streams on the server.
  void TerminateAllStreams();

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
      void AddQueue(std::queue<std::string>* queue) {
        {
          std::lock_guard<std::mutex> lk(mutex_);
          queues_.push_back(queue);
        }
        // Notify the condition variable to unblock any calls to
        // WaitForOneStreamWatcher().
        cv_.notify_all();
      }

      bool WaitForOneWatcher(time::seconds timeout) {
        std::unique_lock<std::mutex> queues_lock(mutex_);
        return cv_.wait_for(queues_lock,
                            timeout,
                            [this]{ return this->queues_.size() > 0; });
      }

      void SendToAllQueues(const std::string& response) {
        {
          std::lock_guard<std::mutex> lk(mutex_);
          for (auto* queue : queues_) {
            queue->push(response);
          }
        }
        cv_.notify_all();
      }

      std::string GetNextResponse(std::queue<std::string>* queue) {
        std::unique_lock<std::mutex> lk(mutex_);
        cv_.wait(lk, [&queue]{ return queue->size() > 0; });
        std::string s = queue->front();
        queue->pop();
        lk.unlock();
        return s;
      }

     private:
      std::mutex mutex_;
      std::condition_variable cv_;
      // The vector elements are not owned by the Stream object.
      std::vector<std::queue<std::string>*> queues_;
    };

    void operator()(Server::request const &request,
                    Server::connection_ptr connection);

    std::map<std::string, std::string> path_responses;
    std::map<std::string, Stream> path_streams;
  };

  Handler handler_;
  Server server_;
  std::thread server_thread_;
};

}  // testing
}  // google

#endif  // FAKE_HTTP_SERVER_H_
