#ifndef FAKE_HTTP_SERVER_H_
#define FAKE_HTTP_SERVER_H_

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
  void WaitForOneStreamWatcher(const std::string& path);

  // Sends a streaming response to all watchers fo the given path.
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
    struct Stream {
      std::mutex mutex;
      std::condition_variable cv;
      std::vector<std::queue<std::string>*> queues;
    };

    void operator()(Server::request const &request,
                    Server::connection_ptr connection);

    std::map<std::string, std::string> path_responses;
    std::map<std::string, std::unique_ptr<Stream>> path_streams;
  };

  Handler handler_;
  Server server_;
  std::thread server_thread_;
};

}  // testing
}  // google

#endif  // FAKE_HTTP_SERVER_H_
