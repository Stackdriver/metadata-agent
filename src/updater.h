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
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "api_server.h"
#include "environment.h"
#include "resource.h"

namespace google {

// A base class for all periodic updates of the metadata mapping.
// All specific metadata updaters inherit from this.
class PollingMetadataUpdater {
 public:
  struct ResourceMetadata {
    ResourceMetadata(const std::vector<std::string>& ids_,
                     const MonitoredResource& resource_,
                     MetadataAgent::Metadata&& metadata_)
        : ids(ids_), resource(resource_), metadata(std::move(metadata_)) {}
    std::vector<std::string> ids;
    MonitoredResource resource;
    MetadataAgent::Metadata metadata;
  };

  PollingMetadataUpdater(
      double period_s, MetadataAgent* store,
      std::function<std::vector<ResourceMetadata>()> query_metadata);
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
  std::function<std::vector<ResourceMetadata>()> query_metadata_;

  // The timer.
  std::timed_mutex timer_;

  // The thread that polls for new metadata.
  std::thread reporter_thread_;
};

class DockerReader {
 public:
  DockerReader(const MetadataAgentConfiguration& config);
  // A Docker metadata query function.
  std::vector<PollingMetadataUpdater::ResourceMetadata> MetadataQuery() const;

 private:
  const MetadataAgentConfiguration& config_;
  Environment environment_;
};

}

#endif  // UPDATER_H_
