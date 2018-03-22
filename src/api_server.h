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
#ifndef API_SERVER_H_
#define API_SERVER_H_

//#include "config.h"

#include <chrono>
#include <map>
#include <memory>
#include <mutex>
#include <string>

#include "configuration.h"
#include "json.h"
#include "resource.h"
#include "time.h"

namespace google {

// A server that implements the metadata agent API.
class MetadataApiServer;

// A periodic reporter of metadata to Stackdriver.
class MetadataReporter;

// A timestamp type.
using Timestamp = time_point;

// Stores the metadata mapping.
class MetadataStore {
 public:
  struct Metadata {
    Metadata(const std::string& version_,
             bool is_deleted_,
             const Timestamp& created_at_,
             const Timestamp& collected_at_,
             json::value metadata_)
        : version(version_), is_deleted(is_deleted_), created_at(created_at_),
          collected_at(collected_at_), metadata(std::move(metadata_)),
          ignore(false) {}
    Metadata(Metadata&& other)
        : version(other.version), is_deleted(other.is_deleted),
          created_at(other.created_at), collected_at(other.collected_at),
          metadata(std::move(other.metadata)), ignore(other.ignore) {}

    Metadata Clone() const {
      if (ignore) {
        return {};
      }
      return {version, is_deleted, created_at, collected_at, metadata->Clone()};
    }

    static Metadata IGNORED();

    const std::string version;
    const bool is_deleted;
    const Timestamp created_at;
    const Timestamp collected_at;
    json::value metadata;
    const bool ignore;

   private:
    Metadata()
        : version(), is_deleted(false), created_at(), collected_at(),
          metadata(json::object({})), ignore(true) {}
  };

  MetadataStore(const MetadataAgentConfiguration& config);

  // Updates the local resource map entry for a given resource.
  // Each local id in `resource_ids` is effectively an alias for `resource`.
  // Adds a resource mapping from each of the `resource_ids` to the `resource`.
  void UpdateResource(const std::vector<std::string>& resource_ids,
                      const MonitoredResource& resource);

  // Updates metadata for a given resource.
  // Adds a metadata mapping from the `resource` to the metadata `entry`.
  void UpdateMetadata(const MonitoredResource& resource,
                      Metadata&& entry);

 private:
  friend class MetadataApiServer;
  friend class MetadataReporter;

  std::map<MonitoredResource, Metadata> GetMetadataMap() const;
  void PurgeDeletedEntries();

  const MetadataAgentConfiguration& config_;

  // A lock that guards access to the local resource map.
  mutable std::mutex resource_mu_;
  // A map from a locally unique id to MonitoredResource.
  std::map<std::string, MonitoredResource> resource_map_;
  // A lock that guards access to the metadata map.
  mutable std::mutex metadata_mu_;
  // A map from MonitoredResource to (JSON) resource metadata.
  std::map<MonitoredResource, Metadata> metadata_map_;
};

// Runs the metadata tasks.
class MetadataAgent {
 public:
  using Metadata = MetadataStore::Metadata;

  MetadataAgent(const MetadataAgentConfiguration& config);
  ~MetadataAgent();

  // Starts serving.
  void start();

  const MetadataAgentConfiguration& config() const {
    return config_;
  }

  const MetadataStore& store() const {
    return store_;
  }

  MetadataStore* mutable_store() {
    return &store_;
  }

 private:
  const MetadataAgentConfiguration& config_;

  // The store for the metadata.
  MetadataStore store_;

  // The Metadata API server.
  std::unique_ptr<MetadataApiServer> metadata_api_server_;
  // The metadata reporter.
  std::unique_ptr<MetadataReporter> reporter_;
};

}

#endif  // API_SERVER_H_
