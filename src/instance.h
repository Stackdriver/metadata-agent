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
#ifndef INSTANCE_H_
#define INSTANCE_H_

//#include "config.h"

#include <vector>

#include "configuration.h"
#include "environment.h"
#include "resource.h"
#include "updater.h"

namespace google {

class InstanceReader {
 public:
  InstanceReader(const MetadataAgentConfiguration& config);
  // A Instance metadata query function.
  std::vector<MetadataUpdater::ResourceMetadata> MetadataQuery() const;

  // Gets the monitored resource of the instance the agent is running on.
  static MonitoredResource InstanceResource(const Environment& environment);

 private:
  const MetadataAgentConfiguration& config_;
  Environment environment_;
};

class InstanceUpdater : public PollingMetadataUpdater {
 public:
  InstanceUpdater(MetadataAgent* server)
      : reader_(server->config()), PollingMetadataUpdater(
          server, server->config().InstanceUpdaterIntervalSeconds(),
          std::bind(&google::InstanceReader::MetadataQuery, &reader_)) { }
 private:
  InstanceReader reader_;
};

}

#endif  // INSTANCE_H_
