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

//#include "config.h"

#include <vector>

#include "configuration.h"
#include "environment.h"
#include "updater.h"

namespace google {

class DockerReader {
 public:
  DockerReader(const MetadataAgentConfiguration& config);
  // A Docker metadata query function.
  std::vector<MetadataUpdater::ResourceMetadata> MetadataQuery() const;

  // Validates that the reader is configured properly.
  // Returns a bool that represents if it's configured properly.
  const bool IsConfigured() const;

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

  const MetadataAgentConfiguration& config_;
  Environment environment_;
};

class DockerUpdater : public PollingMetadataUpdater {
 public:
  DockerUpdater(MetadataAgent* server)
      : reader_(server->config()), PollingMetadataUpdater(
          server, server->config().DockerUpdaterIntervalSeconds(),
          std::bind(&google::DockerReader::MetadataQuery, &reader_)) { }

  void start();
 private:
  DockerReader reader_;
};

}

#endif  // DOCKER_H_
