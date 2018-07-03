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

#include "store.h"

#include "configuration.h"
#include "logging.h"
#include "time.h"

namespace google {

MetadataStore::Metadata MetadataStore::Metadata::IGNORED() {
  return MetadataStore::Metadata();
}

MetadataStore::MetadataStore(const Configuration& config) : config_(config) {}

const MonitoredResource& MetadataStore::LookupResource(
    const std::string& resource_id) const throw(std::out_of_range) {
  std::lock_guard<std::mutex> lock(resource_mu_);
  return resource_map_.at(resource_id);
}

void MetadataStore::UpdateResource(const std::vector<std::string>& resource_ids,
                                   const MonitoredResource& resource) {
  std::lock_guard<std::mutex> lock(resource_mu_);
  // TODO: How do we handle deleted resources?
  // TODO: Do we care if the value was already there?
  for (const std::string& id : resource_ids) {
    if (config_.VerboseLogging()) {
      LOG(INFO) << "Updating resource map '" << id << "'->" << resource;
    }
    resource_map_.emplace(id, resource);
  }
}

void MetadataStore::UpdateMetadata(const std::string& full_resource_name,
                                   Metadata&& entry) {
  if (entry.ignore) {
    return;
  }
  std::lock_guard<std::mutex> lock(metadata_mu_);
  if (config_.VerboseLogging()) {
    LOG(INFO) << "Updating metadata map " << full_resource_name << "->{"
              << "type: " << entry.type << ", "
              << "location: " << entry.location << ", "
              << "version: " << entry.version << ", "
              << "schema name: " << entry.schema_name << ", "
              << "is_deleted: " << entry.is_deleted << ", "
              << "collected_at: " << time::rfc3339::ToString(entry.collected_at)
              << ", "
              << "metadata: " << *entry.metadata << ", "
              << "ignore: " << entry.ignore
              << "}";
  }
  // Force value update. The repeated search is inefficient, but shouldn't
  // be a huge deal.
  metadata_map_.erase(full_resource_name);
  metadata_map_.emplace(full_resource_name, std::move(entry));

  auto found = last_collection_times_.emplace(entry.type, entry.collected_at);
  // Force timestamp update for existing entries.
  if (!found.second && found.first->second < entry.collected_at) {
    found.first->second = entry.collected_at;
  }
}

std::map<std::string, MetadataStore::Metadata>
    MetadataStore::GetMetadataMap() const {
  std::lock_guard<std::mutex> lock(metadata_mu_);

  std::map<std::string, Metadata> result;
  for (const auto& kv : metadata_map_) {
    const std::string& full_resource_name = kv.first;
    const Metadata& metadata = kv.second;
    result.emplace(full_resource_name, metadata.Clone());
  }
  return result;
}

std::map<std::string, Timestamp> MetadataStore::GetLastCollectionTimes() const {
  std::lock_guard<std::mutex> lock(metadata_mu_);
  return last_collection_times_;
}

void MetadataStore::PurgeDeletedEntries() {
  std::lock_guard<std::mutex> lock(metadata_mu_);

  for (auto it = metadata_map_.begin(); it != metadata_map_.end(); ) {
    const std::string& full_resource_name = it->first;
    const Metadata& entry = it->second;
    if (entry.is_deleted) {
      if (config_.VerboseLogging()) {
        LOG(INFO) << "Purging metadata entry " << full_resource_name << "->{"
                  << "type: " << entry.type << ", "
                  << "location: " << entry.location << ", "
                  << "version: " << entry.version << ", "
                  << "schema name: " << entry.schema_name << ", "
                  << "is_deleted: " << entry.is_deleted << ", "
                  << "collected_at: " << time::rfc3339::ToString(entry.collected_at)
                  << ", "
                  << "metadata: " << *entry.metadata << ", "
                  << "ignore: " << entry.ignore
                  << "}";
      }
      it = metadata_map_.erase(it);
    } else {
      ++it;
    }
  }
}

}
