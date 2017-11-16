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

  MetadataUpdater(MetadataAgent* store);
  virtual ~MetadataUpdater();

  const MetadataAgentConfiguration& config() {
    return store_->config();
  }

  // Starts updating.
  virtual void start() = 0;

  // Stops updating.
  virtual void stop() = 0;

  using UpdateCallback =
      std::function<void(std::vector<MetadataUpdater::ResourceMetadata>&&)>;

 protected:
  // Updates the resource map in the store.
  void UpdateResourceCallback(const ResourceMetadata& result) {
    store_->UpdateResource(result.ids, result.resource);
  }

  // Updates the metadata in the store. Consumes result.
  void UpdateMetadataCallback(ResourceMetadata&& result) {
    store_->UpdateMetadata(result.resource, std::move(result.metadata));
  }

 private:
  // The store for the metadata.
  MetadataAgent* store_;
};

// A class for all periodic updates of the metadata mapping.
class PollingMetadataUpdater : public MetadataUpdater {
 public:
  PollingMetadataUpdater(
      MetadataAgent* store, double period_s,
      std::function<std::vector<ResourceMetadata>()> query_metadata);
  ~PollingMetadataUpdater();

  void start();
  void stop();

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
