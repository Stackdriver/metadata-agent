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

#include "../src/configuration.h"
#include "../src/environment.h"
#include "../src/instance.h"
#include "../src/resource.h"
#include "../src/updater.h"
#include "gtest/gtest.h"

namespace google {
namespace {

TEST(InstanceTest, GetInstanceMonitoredResource) {
  Configuration config(std::istringstream(
      "InstanceResourceType: gce_instance\n"
      "InstanceId: 1234567891011\n"
      "InstanceZone: us-east1-b\n"
  ));
  Environment env(config);
  EXPECT_EQ(MonitoredResource("gce_instance", {
    {"instance_id", "1234567891011"},
    {"zone", "us-east1-b"}
  }), InstanceReader::InstanceResource(env));
}

TEST(InstanceTest, GetInstanceMetatadataQuery) {
  Configuration config(std::istringstream(
      "InstanceResourceType: gce_instance\n"
      "InstanceId: 1234567891011\n"
      "InstanceZone: us-east1-b\n"
  ));
  InstanceReader reader(config);
  const auto result = reader.MetadataQuery();
  EXPECT_EQ(1, result.size());
  const MetadataUpdater::ResourceMetadata& rm = result[0];
  EXPECT_EQ(MonitoredResource("gce_instance", {
    {"instance_id", "1234567891011"},
    {"zone", "us-east1-b"}
  }), rm.resource());
  EXPECT_EQ(std::vector<std::string>({"", "1234567891011"}), rm.ids());
  EXPECT_TRUE(rm.metadata().ignore);
}

}  // namespace
}  // google
