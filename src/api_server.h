#ifndef API_SERVER_H_
#define API_SERVER_H_

//#include "config.h"

#include <map>
#include <memory>
#include <mutex>
#include <string>

#include "resource.h"

namespace google {

// A server that implements the metadata agent API.
class MetadataApiServer;

// A periodic reporter of metadata to Stackdriver.
class MetadataReporter;

// Stores the metadata mapping and runs the metadata tasks.
class MetadataAgent {
 public:
  MetadataAgent();

  // Updates metadata for a given resource.
  void UpdateResource(const std::string& id, const MonitoredResource& resource,
                      const std::string& metadata);

  // Starts serving.
  void start();

  ~MetadataAgent();

 private:
  friend class MetadataApiServer;
  friend class MetadataReporter;

  // A lock that guards access to the maps.
  std::mutex mu_;
  // A map from a locally unique id to MonitoredResource.
  std::map<std::string, MonitoredResource> resource_map_;
  // A map from MonitoredResource to (JSON) resource metadata.
  std::map<MonitoredResource, std::string> metadata_map_;

  // The Metadata API server.
  std::unique_ptr<MetadataApiServer> metadata_api_server_;
  // The metadata reporter.
  std::unique_ptr<MetadataReporter> reporter_;
};

}

#endif  // API_SERVER_H_
