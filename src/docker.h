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
#ifndef DOCKER_H_
#define DOCKER_H_

#include <vector>

#include "environment.h"
#include "updater.h"

namespace google {

// Configuration object.
class Configuration;

// Storage for the metadata mapping.
class MetadataStore;

class DockerReader {
 public:
  DockerReader(const Configuration& config);
  // A Docker metadata query function.
  std::vector<MetadataUpdater::ResourceMetadata> MetadataQuery() const;

  // Validates the relevant configuration and returns true if it's correct.
  // Returns a bool that represents if it's configured properly.
  bool ValidateConfiguration() const;

 private:
  // A representation of all query-related errors.
  class QueryException {
   public:
    QueryException(const std::string& what) : explanation_(what) {}
    const std::string& what() const { return explanation_; }
   private:
    std::string explanation_;
  };

  // Issues a Docker API query at a given path and returns a parsed
  // JSON response. The path has to start with "/".
  json::value QueryDocker(const std::string& path) const
      throw(QueryException, json::Exception);

  // Given a container object, return the associated metadata.
  MetadataUpdater::ResourceMetadata GetContainerMetadata(
      const json::Object* container, Timestamp collected_at) const
      throw(json::Exception);

  const Configuration& config_;
  Environment environment_;
};

class DockerUpdater : public PollingMetadataUpdater {
 public:
  DockerUpdater(const Configuration& config, MetadataStore* store)
      : reader_(config), PollingMetadataUpdater(
          config, store, "DockerUpdater",
          config.DockerUpdaterIntervalSeconds(),
          [=]() { return reader_.MetadataQuery(); }) { }

 protected:
  bool ValidateConfiguration() const;

 private:
  DockerReader reader_;
};

}

#endif  // DOCKER_H_
