#ifndef UPDATER_H_
#define UPDATER_H_

//#include "config.h"

#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <utility>

#include "resource.h"
#include "api_server.h"

namespace google {

// A base class for all periodic updates of the metadata mapping.
// All specific metadata updaters inherit from this.
class PollingMetadataUpdater {
 public:
  PollingMetadataUpdater(double period_s, MetadataApiServer* store);

  // Starts updating.
  void start();

 protected:
  virtual std::tuple<std::string, MonitoredResource, std::string>
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
 public:
  DockerMetadataUpdater(double period_s, MetadataApiServer* store)
      : PollingMetadataUpdater(period_s, store) {}
 protected:
  std::tuple<std::string, MonitoredResource, std::string> QueryMetadata()
      override;
};

}

#endif  // UPDATER_H_
