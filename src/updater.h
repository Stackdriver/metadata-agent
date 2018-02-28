/*
 * Copyright 2017 Google Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 **/
#ifndef UPDATER_H_
#define UPDATER_H_

//#include "config.h"

#include <chrono>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "api_server.h"
#include "resource.h"

namespace google {

// An abstract class for asynchronous updates of the metadata mapping.
class MetadataUpdater {
 public:
  struct ResourceMetadata {
    ResourceMetadata(const std::vector<std::string>& ids_,
                     const MonitoredResource& resource_,
                     MetadataAgent::Metadata&& metadata_)
        : ids(ids_), resource(resource_), metadata(std::move(metadata_)) {}
    ResourceMetadata(ResourceMetadata&& other)
        : ResourceMetadata(other.ids, other.resource,
                           std::move(other.metadata)) {}
    std::vector<std::string> ids;
    MonitoredResource resource;
    MetadataAgent::Metadata metadata;
  };

  MetadataUpdater(MetadataAgent* store, const std::string& name);
  virtual ~MetadataUpdater();

  const MetadataAgentConfiguration& config() {
    return store_->config();
  }

  // Starts updating.
  void start();

  // Stops updating.
  void stop();

  using UpdateCallback =
      std::function<void(std::vector<MetadataUpdater::ResourceMetadata>&&)>;

 protected:
  // Validates the relevant configuration and returns true if it's correct.
  // Returns a bool that represents if it's configured properly.
  virtual bool ValidateConfiguration() const {
    return true;
  }

  // Internal method for starting the updater's logic.
  virtual void StartUpdater() = 0;

  // Internal method for stopping the updater's logic.
  virtual void StopUpdater() = 0;

  // Updates the resource map in the store.
  void UpdateResourceCallback(const ResourceMetadata& result) {
    store_->UpdateResource(result.ids, result.resource);
  }

  // Updates the metadata in the store. Consumes result.
  void UpdateMetadataCallback(ResourceMetadata&& result) {
    store_->UpdateMetadata(result.resource, std::move(result.metadata));
  }

 private:
  // The name of the updater provided by subclasses.
  std::string name_;

  // The store for the metadata.
  MetadataAgent* store_;
};

// A class for all periodic updates of the metadata mapping.
class PollingMetadataUpdater : public MetadataUpdater {
 public:
  PollingMetadataUpdater(
      MetadataAgent* store, const std::string& name, double period_s,
      std::function<std::vector<ResourceMetadata>()> query_metadata);
  ~PollingMetadataUpdater();

 protected:
  void StartUpdater();
  void StopUpdater();

 private:
  using seconds = std::chrono::duration<double, std::chrono::seconds::period>;
  // Metadata poller.
  void PollForMetadata();

  // The polling period in seconds.
  seconds period_;

  // The function to actually query for metadata.
  std::function<std::vector<ResourceMetadata>()> query_metadata_;

  // The timer.
  std::timed_mutex timer_;

  // The thread that polls for new metadata.
  std::thread reporter_thread_;
};

}

#endif  // UPDATER_H_
