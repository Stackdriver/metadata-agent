#ifndef UPDATER_H_
#define UPDATER_H_

//#include "config.h"

#include <chrono>
#include <mutex>
#include <string>
#include <thread>

#include "resource.h"
#include "api_server.h"

namespace google {

// A base class for all periodic updates of the metadata mapping.
// All specific metadata updaters inherit from this.
class PollingMetadataUpdater {
 public:
  struct Metadata {
    Metadata(const std::string& id_,
             const MonitoredResource& resource_,
             const std::string& metadata_)
        : id(id_), resource(resource_), metadata(metadata_) {}
    std::string id;
    MonitoredResource resource;
    std::string metadata;
  };

  PollingMetadataUpdater(double period_s, MetadataAgent* store,
                         std::function<Metadata(void)> query_metadata);
  ~PollingMetadataUpdater();

  // Starts updating.
  void start();

  // Stops updating.
  void stop();

 private:
  using seconds = std::chrono::duration<double, std::chrono::seconds::period>;
  // Metadata poller.
  void PollForMetadata();

  // The polling period in seconds.
  seconds period_;

  // The store for the metadata.
  MetadataAgent* store_;

  // The function to actually query for metadata.
  std::function<Metadata(void)> query_metadata_;

  // The timer.
  std::timed_mutex timer_;

  // The thread that polls for new metadata.
  std::thread reporter_thread_;
};

// A Docker metadata query function.
PollingMetadataUpdater::Metadata DockerMetadataQuery();

}

#endif  // UPDATER_H_
