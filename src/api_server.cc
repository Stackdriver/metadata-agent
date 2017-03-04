#include "api_server.h"

#include "logging.h"

#include <boost/network/protocol/http/server.hpp>
#include <boost/range/irange.hpp>
#include <ostream>
#include <thread>

namespace http = boost::network::http;

namespace google {

class MetadataApiServer {
 public:
  MetadataApiServer(const MetadataAgent& agent, int server_threads,
                    const std::string& host, int port);
  ~MetadataApiServer();

 private:
  class Handler;
  using HttpServer = http::server<Handler>;
  class Handler {
   public:
    Handler(const MetadataAgent& agent);
    void operator()(const HttpServer::request& request,
                    HttpServer::response& response);
    void log(const HttpServer::string_type& info);
   private:
    const MetadataAgent& agent_;
  };
  friend std::ostream& operator<<(
      std::ostream& o, const HttpServer::request::headers_container_type& hv);

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


// To allow logging headers.
std::ostream& operator<<(
    std::ostream& o,
    const MetadataApiServer::HttpServer::request::headers_container_type& hv) {
  o << "[";
  for (const auto& h : hv) {
    o << " " << h.name << ": " << h.value;
  }
  o << "]";
}


MetadataApiServer::Handler::Handler(const MetadataAgent& agent)
    : agent_(agent) {}

void MetadataApiServer::Handler::operator()(const HttpServer::request& request,
                                            HttpServer::response& response) {
  static const std::string kPrefix = "/monitoredResource/";
  // The format for the local metadata API request is:
  //   {host}:{port}/monitoredResource/{id}
  LOG(INFO) << "Handler called: " << request.method
            << " " << request.destination
            << " headers: " << request.headers
            << " body: " << request.body;
  if (request.method == "GET" && request.destination.find(kPrefix) == 0) {
    std::string id = request.destination.substr(kPrefix.size());
    const auto result = agent_.resource_map_.find(id);
    if (result == agent_.resource_map_.end()) {
      LOG(ERROR) << "No matching resource for " << id;
      response = HttpServer::response::stock_reply(
          HttpServer::response::not_found, "");
    } else {
      const MonitoredResource& resource = result->second;
      LOG(INFO) << "Found resource for " << id << ": " << resource;
      response = HttpServer::response::stock_reply(
          HttpServer::response::ok, resource.ToJSON());
    }
  }
}

void MetadataApiServer::Handler::log(const HttpServer::string_type& info) {
  LOG(ERROR) << info;
}


MetadataApiServer::MetadataApiServer(const MetadataAgent& agent,
                                     int server_threads,
                                     const std::string& host, int port)
    : handler_(agent),
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
