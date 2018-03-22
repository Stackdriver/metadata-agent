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
#ifndef AGENT_H_
#define AGENT_H_

//#include "config.h"

#include <memory>

#include "configuration.h"
#include "store.h"

namespace google {

// A server that implements the metadata agent API.
class MetadataApiServer;

// A periodic reporter of metadata to Stackdriver.
class MetadataReporter;

// Runs the metadata tasks.
class MetadataAgent {
 public:
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

#endif  // AGENT_H_
