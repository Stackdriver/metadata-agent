#include "config.h"

#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <utility>

namespace google {

class MonitoredResource {
 public:
  MonitoredResource(const std::string& type,
                    const std::map<std::string, std::string> labels)
      : type_(type), labels_(labels) {}
  const std::string& type() { return type_; }
  const std::map<std::string, std::string>& labels() { return labels_; }
  bool operator==(const MonitoredResource& other) {
    return other.type_ == type_ && other.labels_ == labels_;
  }

  std::string ToJSON();
  static MonitoredResource FromJSON(const std::string&);

 private:
  std::string type_;
  std::map<std::string, std::string> labels_;
};

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

// A base class for all periodic updates of the metadata mapping.
// All specific metadata updaters inherit from this.
class PollingMetadataUpdater {
 public:
  PollingMetadataUpdater(double period_s, MetadataApiServer* store);

  // Starts updating.
  void start();

 protected:
  virtual std::tuple<std::string, MonitoredResource, std::tuple>
  QueryMetadata() = 0;

 private:
  // Metadata poller.
  void PollForMetadata();

  // The polling period in seconds.
  double period_s_;
  // The store for the metadata.
  MetadataApiServer* store_;

  // The thread that polls for new metadata.
  std::unique_ptr<std::thread> reporter_thread;
};

// A Docker metadata updater.
class DockerMetadataUpdater : public PollingMetadataUpdater {
 protected:
  std::tuple<std::string, MonitoredResource, std::tuple> QueryMetadata()
      override;
};

int main(int ac, char** av) {
  MetadataApiServer server;
  DockerMetadataUpdater docker_updater(60.0, &server);

  docker_updater.start();
  server.start();
}
