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
