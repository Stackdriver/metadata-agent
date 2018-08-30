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

void MetadataUpdater::Start() throw(ConfigurationValidationError) {
  ValidateStaticConfiguration();

  if (ShouldStartUpdater()) {
    ValidateDynamicConfiguration();
    StartUpdater();
  } else {
    LOG(INFO) << "Not starting " << name_;
  }
}

void MetadataUpdater::NotifyStop() {
  NotifyStopUpdater();
}

PollingMetadataUpdater::PollingMetadataUpdater(
    const Configuration& config, MetadataStore* store,
    const std::string& name, double period_s,
    std::function<std::vector<ResourceMetadata>()> query_metadata)
    : MetadataUpdater(config, store, name),
      timer_(new TimerImpl<std::chrono::high_resolution_clock>(
         period_s, config.VerboseLogging(), name)),
      query_metadata_(query_metadata),
      reporter_thread_() {}

PollingMetadataUpdater::PollingMetadataUpdater(
    const Configuration& config, MetadataStore* store,
    const std::string& name, std::unique_ptr<Timer> timer,
    std::function<std::vector<ResourceMetadata>()> query_metadata)
    : MetadataUpdater(config, store, name),
      timer_(std::move(timer)),
      query_metadata_(query_metadata),
      reporter_thread_() {}

PollingMetadataUpdater::~PollingMetadataUpdater() {
  if (reporter_thread_.joinable()) {
    reporter_thread_.join();
  }
}

void PollingMetadataUpdater::ValidateStaticConfiguration() const
    throw(ConfigurationValidationError) {
  if (timer_->Period() < std::chrono::seconds::zero()) {
    throw ConfigurationValidationError(
        format::Substitute("Polling period {{period}}s cannot be negative",
                           {{"period", format::str(int(timer_->Period().count()))}}));
  }
}

bool PollingMetadataUpdater::ShouldStartUpdater() const {
  return timer_->Period() > std::chrono::seconds::zero();
}

void PollingMetadataUpdater::StartUpdater() {
  timer_->Init();
  reporter_thread_ = std::thread([=]() { PollForMetadata(); });
}

void PollingMetadataUpdater::NotifyStopUpdater() {
  timer_->Cancel();
}

void PollingMetadataUpdater::PollForMetadata() {
  do {
    std::vector<ResourceMetadata> result_vector = query_metadata_();
    for (ResourceMetadata& result : result_vector) {
      UpdateResourceCallback(result);
      UpdateMetadataCallback(std::move(result));
    }
  } while (timer_->Wait());
  if (config().VerboseLogging()) {
    LOG(INFO) << "Timer unlocked (stop polling) for " << name();
  }
}

}
