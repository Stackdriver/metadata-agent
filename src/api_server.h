#ifndef API_SERVER_H_
#define API_SERVER_H_

//#include "config.h"

#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#include "resource.h"

namespace google {

// A server that stores the metadata mapping and implements the metadata agent
// API.
class MetadataApiServer {
 public:
  MetadataApiServer() = default;

  // Updates metadata for a given resource.
  void UpdateResource(const std::string& id, const MonitoredResource& resource,
                      const std::string& metadata);

  // Starts serving.
  void start();

  ~MetadataApiServer();

 private:
  // Metadata API implementation.
  void ServeMetadataAPI();
  // Metadata reporter.
  void ReportMetadata();

  // A lock that guards access to the maps.
  std::mutex mu_;
  // A map from a locally unique id to MonitoredResource.
  std::map<std::string, MonitoredResource> resource_map_;
  // A map from MonitoredResource to (JSON) resource metadata.
  std::map<MonitoredResource, std::string> metadata_map_;

  // The server thread that listens to a socket.
  std::unique_ptr<std::thread> server_thread;
  // The thread that reports new metadata to Stackdriver.
  std::unique_ptr<std::thread> reporter_thread;
};

}

#endif  // API_SERVER_H_
