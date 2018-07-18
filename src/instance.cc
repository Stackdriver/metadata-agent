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

#include "instance.h"

#include <boost/network/protocol/http/client.hpp>
#include <chrono>

#include "configuration.h"
#include "json.h"
#include "logging.h"
#include "store.h"
#include "time.h"

namespace http = boost::network::http;

namespace google {

InstanceReader::InstanceReader(const Configuration& config)
    : config_(config), environment_(config) {}

/*static*/
MonitoredResource InstanceReader::InstanceResource(const Environment& environment) {
  const std::string resource_type = environment.InstanceResourceType();
  const std::string instance_id = environment.InstanceId();
  const std::string zone = environment.InstanceZone();
  return {resource_type, {
    {"instance_id", instance_id},
    {"zone", zone},
  }};
}

std::vector<MetadataUpdater::ResourceMetadata>
    InstanceReader::MetadataQuery() const {
  if (config_.VerboseLogging()) {
    LOG(INFO) << "Instance Query called";
  }

  MonitoredResource instance_resource = InstanceResource(environment_);

  std::vector<MetadataUpdater::ResourceMetadata> result;
  result.emplace_back(
      std::vector<std::string>{"", environment_.InstanceId()},
      instance_resource,
      // TODO: Send actual instance metadata.
      MetadataStore::Metadata::IGNORED()
  );
  return result;
}

}

