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

#include "updater.h"

#include <chrono>

#include "configuration.h"
#include "logging.h"

namespace google {

MetadataUpdater::MetadataUpdater(const Configuration& config,
                                 MetadataStore* store, const std::string& name)
    : config_(config), store_(store), name_(name) {}

MetadataUpdater::~MetadataUpdater() {}

void MetadataUpdater::start() throw(ConfigurationValidationError) {
  ValidateStaticConfiguration();

  if (ShouldStartUpdater()) {
    ValidateDynamicConfiguration();
    StartUpdater();
  } else {
    LOG(INFO) << "Not starting " << name_;
  }
}

void MetadataUpdater::stop() {
  StopUpdater();
}

}
