#include "api_server.h"

namespace google {

void MetadataApiServer::UpdateResource(const std::string& id,
                                       const MonitoredResource& resource,
                                       const std::string& metadata) {
  std::lock_guard<std::mutex> lock(mu_);

  // TODO: Add "collected_at".
  // TODO: How do we handle deleted resources?
  // TODO: Do we care if the value was already there?
  resource_map_.insert(id, resource);
  metadata_map_.insert(resource, metadata);
}

void MetadataApiServer::start() {
  server_thread_.reset(new std::thread(ServeMetadataAPI));
}

}
