#include "fake_http_server.h"

#include "../src/json.h"
#include "../src/logging.h"

namespace google {
namespace testing {

FakeServer::FakeServer()
    // Note: An empty port selects a random available port (this behavior
    // is not documented).
    : server_(Server::options(handler_)
                .thread_pool(std::make_shared<boost::network::utils::thread_pool>(5))
                .address("127.0.0.1")
                .port("")) {
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

void FakeServer::AllowStream(const std::string& path) {
  handler_.path_streams[path] = std::unique_ptr<Handler::Stream>(
      new Handler::Stream);
}

void FakeServer::WaitForOneStreamWatcher(const std::string& path) {
  auto s = handler_.path_streams.find(path);
  if (s == handler_.path_streams.end()) {
    LOG(ERROR) << "No stream for path " << path;
    return;
  }
  auto& stream = s->second;
  std::unique_lock<std::mutex> lk(stream->mutex);
  stream->cv.wait(lk, [&stream]{return stream->queues.size() > 0;});
  lk.unlock();
}

void FakeServer::SendStreamResponse(const std::string& path, const std::string& response) {
  auto s = handler_.path_streams.find(path);
  if (s == handler_.path_streams.end()) {
    LOG(ERROR) << "No stream for path " << path;
    return;
  }
  auto& stream = s->second;
  {
    std::lock_guard<std::mutex> lk(stream->mutex);
    for (auto* queue : stream->queues) {
      queue->push(response);
    }
  }
  stream->cv.notify_all();
}

void FakeServer::TerminateAllStreams() {
  // Send sentinel (empty string) to all queues.
  for (auto& s : handler_.path_streams) {
    auto& stream = s.second;
    {
      std::lock_guard<std::mutex> lk(stream->mutex);
      for (auto* queue : stream->queues) {
        queue->push("");
      }
    }
    stream->cv.notify_all();
  }
}

void FakeServer::Handler::operator()(Server::request const &request,
                                     Server::connection_ptr connection) {
  auto s = path_streams.find(request.destination);
  if (s != path_streams.end()) {
    auto& stream = s->second;
    connection->set_status(Server::connection::ok);
    connection->set_headers(std::map<std::string, std::string>({
        {"Content-Type", "text/plain"},
    }));

    // Create a queue for this watcher, and notify the condition
    // variable to unblock any calls to WaitForOneStreamWatcher().
    std::queue<std::string> my_queue;
    {
      std::lock_guard<std::mutex> lk(stream->mutex);
      stream->queues.push_back(&my_queue);
    }
    stream->cv.notify_all();

    // For every new string on my queue, send to the client.  The
    // empty string indicates that we should terminate the stream.
    while (true) {
      std::unique_lock<std::mutex> lk(stream->mutex);
      stream->cv.wait(lk, [&my_queue]{ return my_queue.size() > 0;});
      std::string s = my_queue.front();
      my_queue.pop();
      lk.unlock();
      if (s.empty()) {
        break;
      }
      connection->write(s);
    }
    return;
  }

  auto it = path_responses.find(request.destination);
  if (it != path_responses.end()) {
    connection->set_status(Server::connection::ok);
    connection->set_headers(std::map<std::string, std::string>({
        {"Content-Type", "text/plain"},
    }));
    connection->write(it->second);
    return;
  }

  // Note: We have to set headers; otherwise, an exception is thrown.
  connection->set_status(Server::connection::not_found);
  connection->set_headers(std::map<std::string, std::string>());
}

}  // testing
}  // google
