#include "fake_http_server.h"

namespace google {
namespace testing {

FakeServerThread::FakeServerThread()
    : server_(Server::options(handler_).address("127.0.0.1").port("")) {
  server_.listen();
  server_thread_ = std::thread([this] { server_.run(); });
}

FakeServerThread::~FakeServerThread() {
  server_.stop();
  server_thread_.join();
}

std::string FakeServerThread::GetUrl() {
  return std::string("http://") + server_.address() + ":" + server_.port() + "/";
}

void FakeServerThread::SetResponse(const std::string& path,
                                   const std::string& response) {
  handler_.path_responses[path] = response;
}

void FakeServerThread::Handler::operator() (Server::request const &request,
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
