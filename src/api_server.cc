#include "api_server.h"

#include "logging.h"

#include <boost/network/protocol/http/server.hpp>
#include <boost/range/irange.hpp>
#include <thread>

namespace http = boost::network::http;

namespace google {

class MetadataApiServer {
 public:
  MetadataApiServer(const MetadataAgent& agent, int server_threads,
                    const std::string& host, int port);
  ~MetadataApiServer();

 private:
  struct Handler;
  using HttpServer = http::server<Handler>;
  struct Handler {
    void operator()(const HttpServer::request& request,
                    HttpServer::response& response);
    void log(const HttpServer::string_type& info);
  };

  const MetadataAgent& agent_;
  Handler handler_;
  HttpServer server_;
  std::vector<std::thread> server_pool_;
};

class MetadataReporter {
 public:
  MetadataReporter(const MetadataAgent& agent);
  ~MetadataReporter();

 private:
  void run();

  const MetadataAgent& agent_;
  std::thread reporter_thread_;
};

void MetadataApiServer::Handler::operator()(const HttpServer::request& request,
                                            HttpServer::response& response) {
  // TODO
  LOG(INFO) << "Handler called";
}

void MetadataApiServer::Handler::log(const HttpServer::string_type& info) {
  LOG(ERROR) << info;
}


MetadataApiServer::MetadataApiServer(const MetadataAgent& agent,
                                     int server_threads,
                                     const std::string& host, int port)
    : agent_(agent),
      handler_(),
      server_(
          HttpServer::options(handler_)
              .address(host)
              .port(std::to_string(port))),
      server_pool_()
{
  for (int i : boost::irange(0, server_threads)) {
    server_pool_.emplace_back(std::bind(&HttpServer::run, &server_));
  }
}

MetadataApiServer::~MetadataApiServer() {
  for (auto& thread : server_pool_) {
    thread.join();
  }
}

MetadataReporter::MetadataReporter(const MetadataAgent& agent)
    : agent_(agent),
      reporter_thread_(std::bind(&MetadataReporter::run, this)) {}

MetadataReporter::~MetadataReporter() {
  reporter_thread_.join();
}

void MetadataReporter::run() {
  // TODO
  LOG(INFO) << "Metadata reporter running";
}

MetadataAgent::MetadataAgent() {}

MetadataAgent::~MetadataAgent() {}

void MetadataAgent::UpdateResource(const std::string& id,
                                   const MonitoredResource& resource,
                                   const std::string& metadata) {
  std::lock_guard<std::mutex> lock(mu_);

  // TODO: Add "collected_at".
  // TODO: How do we handle deleted resources?
  // TODO: Do we care if the value was already there?
  LOG(INFO) << "Updating resource map '" << id << "'->" << resource;
  resource_map_.insert({id, resource});
  LOG(INFO) << "Updating metadata map " << resource << "->'" << metadata << "'";
  metadata_map_.insert({resource, metadata});
}

void MetadataAgent::start() {
  metadata_api_server_.reset(new MetadataApiServer(*this, 3, "0.0.0.0", 8000));
  reporter_.reset(new MetadataReporter(*this));
}

}
