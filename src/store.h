/*
 * Copyright 2018 Google Inc.
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
#ifndef STORE_H_
#define STORE_H_

#include <map>
#include <mutex>
#include <stdexcept>
#include <string>

#include "json.h"
#include "resource.h"
#include "time.h"

namespace google {

// Configuration object.
class Configuration;

// A timestamp type.
using Timestamp = time_point;

// Stores the metadata mapping.
class MetadataStore {
 public:
  struct Metadata {
    Metadata(const std::string& name_,
             const std::string& type_,
             const std::string& location_,
             const std::string& version_,
             const std::string& schema_name_,
             bool is_deleted_,
             const Timestamp& collected_at_,
             json::value metadata_)
        : name(name_), type(type_), location(location_), version(version_),
          schema_name(schema_name_), is_deleted(is_deleted_),
          collected_at(collected_at_),
          metadata(std::move(metadata_)), ignore(false) {}
    Metadata(Metadata&& other)
        : name(other.name), type(other.type), location(other.location),
          version(other.version), schema_name(other.schema_name),
          is_deleted(other.is_deleted), collected_at(other.collected_at),
          metadata(std::move(other.metadata)), ignore(other.ignore) {}
    Metadata(const Metadata& other)
        : name(other.name), type(other.type), location(other.location),
          version(other.version), schema_name(other.schema_name),
          is_deleted(other.is_deleted), collected_at(other.collected_at),
          metadata(other.metadata->Clone()), ignore(other.ignore) {}

    Metadata Clone() const {
      if (ignore) {
        return IGNORED();
      }
      return {name, type, location, version, schema_name, is_deleted,
              collected_at, metadata->Clone()};
    }

    static Metadata IGNORED();

    const std::string name;
    const std::string type;
    const std::string location;
    const std::string version;
    const std::string schema_name;
    const bool is_deleted;
    const Timestamp collected_at;
    json::value metadata;
    const bool ignore;

   private:
    Metadata()
        : name(), type(), location(), version(), schema_name(),
          is_deleted(false), collected_at(), metadata(json::object({})),
          ignore(true) {}
  };

  MetadataStore(const Configuration& config);

  // Returns a copy of the resource metadata.
  std::vector<Metadata> GetMetadata() const;

  // Looks up the local resource map entry for a given resource id.
  // Throws an exception if the resource is not found.
  const MonitoredResource& LookupResource(const std::string& resource_id) const
      throw(std::out_of_range);

  // Updates the local resource map entry for a given resource.
  // Each local id in `resource_ids` is effectively an alias for `resource`.
  // Adds a resource mapping from each of the `resource_ids` to the `resource`.
  void UpdateResource(const std::vector<std::string>& resource_ids,
                      const MonitoredResource& resource);

  // Updates the metadata map with the provided `entry`.
  void UpdateMetadata(Metadata&& entry);

 private:
  friend class MetadataReporter;
  friend class MetadataStoreTest;

  void PurgeDeletedEntries();

  const Configuration& config_;

  // A lock that guards access to the local resource map.
  mutable std::mutex resource_mu_;
  // A map from a locally unique id to MonitoredResource.
  std::map<std::string, MonitoredResource> resource_map_;
  // A lock that guards access to the metadata map.
  mutable std::mutex metadata_mu_;

  // MetadataKey is a pair of the Full Resource Name,
  // https://cloud.google.com/apis/design/resource_names#full_resource_name
  // and the version that uniquely identifies a metadata blob in a cluster.
  using MetadataKey = std::pair<std::string, std::string>;
  // A map from the metadata key to (JSON) resource metadata.
  std::map<MetadataKey, Metadata> metadata_map_;
};

}

#endif  // STORE_H_
