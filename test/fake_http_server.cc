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

#include "../src/json.h"
#include "../src/logging.h"
#include "../src/time.h"

#include <boost/network/utils/thread_pool.hpp>

namespace google {
namespace testing {

FakeServer::FakeServer()
    // Note: An empty port selects a random available port (this behavior
    // is not documented).
    : server_(Server::options(handler_)
                  .thread_pool(
                      std::make_shared<boost::network::utils::thread_pool>(5))
                  .address("127.0.0.1")
                  .port("")) {
  server_.listen();
  server_thread_ = std::thread([this] { server_.run(); });
}

FakeServer::~FakeServer() {
  server_.stop();
  server_thread_.join();
}

/* static */
GetHandler FakeServer::Return(const std::string& body) {
  return [=](const std::string&,
             std::map<std::string, std::string>&) -> std::string {
    return body;
  };
}

std::string FakeServer::GetUrl() {
  network::uri_builder builder;
  builder.scheme("http").host(server_.address()).port(server_.port());
  return builder.uri().string();
}

void FakeServer::SetHandler(const std::string& path, GetHandler handler) {
  handler_.get_handlers[path] = handler;
}

void FakeServer::SetHandler(const std::string& path, PostHandler handler) {
  handler_.post_handlers[path] = handler;
}

void FakeServer::AllowStream(const std::string& path) {
  // Initialize entry for path with default Stream object.
  handler_.path_streams[path];
}

bool FakeServer::WaitForOneStreamWatcher(const std::string& path,
                                         time::seconds timeout) {
  auto stream_it = handler_.path_streams.find(path);
  if (stream_it == handler_.path_streams.end()) {
    LOG(ERROR) << "Attempted to wait for an unknown path " << path;
    return false;
  }
  return stream_it->second.WaitForOneWatcher(timeout);
}

void FakeServer::SendStreamResponse(const std::string& path,
                                    const std::string& response) {
  auto stream_it = handler_.path_streams.find(path);
  if (stream_it == handler_.path_streams.end()) {
    LOG(ERROR) << "No stream for path " << path;
    return;
  }
  stream_it->second.SendToAllQueues(response);
}

void FakeServer::TerminateAllStreams() {
  // Send sentinel (empty string) to all queues.
  for (auto& s : handler_.path_streams) {
    s.second.SendToAllQueues("");
  }
}

void FakeServer::Handler::operator()(Server::request const &request,
                                     Server::connection_ptr connection) {
  if (request.method == "GET") {
    auto stream_it = path_streams.find(request.destination);
    if (stream_it != path_streams.end()) {
      auto& stream = stream_it->second;
      connection->set_status(Server::connection::ok);
      connection->set_headers(std::map<std::string, std::string>({
        {"Content-Type", "text/plain"},
      }));

      // Create a queue for this watcher and add to the stream.
      std::queue<std::string> my_queue;
      stream.AddQueue(&my_queue);

      // For every new string on my queue, send to the client.  The
      // empty string indicates that we should terminate the stream.
      while (true) {
        std::string s = stream.GetNextResponse(&my_queue);
        if (s.empty()) {
          break;
        }
        connection->write(s);
      }
      return;
    }

    auto it = get_handlers.find(request.destination);
    if (it != get_handlers.end()) {
      const auto& handler = it->second;
      std::map<std::string, std::string> headers;
      for (const auto& h : request.headers) {
        headers[h.name] = h.value;
      }
      connection->set_status(Server::connection::ok);
      connection->set_headers(std::map<std::string, std::string>({
        {"Content-Type", "text/plain"},
      }));
      connection->write(handler(request.destination, headers));
      return;
    }
  }

  if (request.method == "POST") {
    auto it = post_handlers.find(request.destination);
    if (it != post_handlers.end()) {
      const auto& handler = it->second;
      std::map<std::string, std::string> headers;
      for (const auto& h : request.headers) {
        headers[h.name] = h.value;
      }
      std::mutex body_mutex;
      std::string body;
      std::condition_variable body_retrieved;
      std::unique_lock<std::mutex> body_lock(body_mutex);
      connection->read([&](Server::connection::input_range range,
                           boost::system::error_code error,
                           size_t size,
                           Server::connection_ptr conn) {
        {
          std::lock_guard<std::mutex> lock(body_mutex);
          body.append(range.begin(), range.end());
        }
        body_retrieved.notify_all();
      });
      bool body_status = body_retrieved.wait_for(
          body_lock, std::chrono::seconds(5),
          [&]() {
            return std::to_string(body.size()) == headers["Content-Length"];
          });
      if (!body_status) {
        LOG(ERROR) << "Reading POST body timed out in fake HTTP server";
        throw std::runtime_error(
            "Reading POST body timed out in fake HTTP server");
      }
      connection->set_status(Server::connection::ok);
      connection->set_headers(std::map<std::string, std::string>({
        {"Content-Type", "text/plain"},
      }));
      connection->write(handler(request.destination, headers, body));
      return;
    }
  }

  // Note: We have to set headers; otherwise, an exception is thrown.
  connection->set_status(Server::connection::not_found);
  connection->set_headers(std::map<std::string, std::string>());
}

void FakeServer::Handler::Stream::AddQueue(std::queue<std::string>* queue) {
  {
    std::lock_guard<std::mutex> lk(mutex_);
    queues_.push_back(queue);
  }
  // Notify the condition variable to unblock any calls to
  // WaitForOneStreamWatcher().
  cv_.notify_all();
}

bool FakeServer::Handler::Stream::WaitForOneWatcher(time::seconds timeout) {
  std::unique_lock<std::mutex> queues_lock(mutex_);
  return cv_.wait_for(queues_lock,
                      timeout,
                      [this]{ return queues_.size() > 0; });
}

void FakeServer::Handler::Stream::SendToAllQueues(const std::string& response) {
  {
    std::lock_guard<std::mutex> lk(mutex_);
    for (auto* queue : queues_) {
      queue->push(response);
    }
  }
  cv_.notify_all();
}

std::string FakeServer::Handler::Stream::GetNextResponse(
    std::queue<std::string>* queue) {
  std::unique_lock<std::mutex> lk(mutex_);
  cv_.wait(lk, [&queue]{ return queue->size() > 0; });
  std::string s = queue->front();
  queue->pop();
  return s;
}

}  // testing
}  // google
