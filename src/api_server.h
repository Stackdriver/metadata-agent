#ifndef API_SERVER_H_
#define API_SERVER_H_

//#include "config.h"

#include <chrono>
#include <map>
#include <memory>
#include <mutex>
#include <string>

#include "json.h"
#include "resource.h"

namespace google {

// A server that implements the metadata agent API.
class MetadataApiServer;

// A periodic reporter of metadata to Stackdriver.
class MetadataReporter;

// A timestamp type.
using Timestamp = std::chrono::time_point<std::chrono::system_clock>;

// Stores the metadata mapping and runs the metadata tasks.
class MetadataAgent {
 public:
  struct Metadata {
    Metadata(const std::string& version_,
             bool is_deleted_,
             const Timestamp& created_at_,
             const Timestamp& collected_at_,
             std::unique_ptr<json::Value> metadata_)
        : version(version_), is_deleted(is_deleted_), created_at(created_at_),
          collected_at(collected_at_), metadata(std::move(metadata_)) {}
    Metadata(Metadata&& other)
        : version(other.version), is_deleted(other.is_deleted),
          created_at(other.created_at), collected_at(other.collected_at),
          metadata(std::move(other.metadata)) {}

    Metadata Clone() const {
      return {version, is_deleted, created_at, collected_at, metadata->Clone()};
    }

    std::string version;
    bool is_deleted;
    Timestamp created_at;
    Timestamp collected_at;
    std::unique_ptr<json::Value> metadata;
  };

  MetadataAgent();

  // Updates metadata for a given resource.
  void UpdateResource(const std::string& resource_id,
                      const MonitoredResource& resource,
                      Metadata&& entry);

  // Starts serving.
  void start();

  ~MetadataAgent();

 private:
  friend class MetadataApiServer;
  friend class MetadataReporter;

  std::map<MonitoredResource, Metadata> GetMetadataMap() const;

  // A lock that guards access to the maps.
  mutable std::mutex mu_;
  // A map from a locally unique id to MonitoredResource.
  std::map<std::string, MonitoredResource> resource_map_;
  // A map from MonitoredResource to (JSON) resource metadata.
  std::map<MonitoredResource, Metadata> metadata_map_;

  // The Metadata API server.
  std::unique_ptr<MetadataApiServer> metadata_api_server_;
  // The metadata reporter.
  std::unique_ptr<MetadataReporter> reporter_;
};

}

#endif  // API_SERVER_H_
