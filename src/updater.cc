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

void MetadataUpdater::start() {
  if (!ValidateConfiguration()) {
    LOG(ERROR) << "Failed to validate configuration for " << name_;
    return;
  }

  StartUpdater();
}

void MetadataUpdater::stop() {
  StopUpdater();
}

PollingMetadataUpdater::PollingMetadataUpdater(
    const Configuration& config, MetadataStore* store,
    const std::string& name, double period_s,
    std::function<std::vector<ResourceMetadata>()> query_metadata)
    : MetadataUpdater(config, store, name),
      period_(period_s),
      query_metadata_(query_metadata),
      timer_(),
      reporter_thread_() {}

PollingMetadataUpdater::~PollingMetadataUpdater() {
  if (reporter_thread_.joinable()) {
    reporter_thread_.join();
  }
}

bool PollingMetadataUpdater::ValidateConfiguration() const {
  return period_ >= time::seconds::zero();
}

void PollingMetadataUpdater::StartUpdater() {
  timer_.lock();
  if (config().VerboseLogging()) {
    LOG(INFO) << "Timer locked";
  }
  if (period_ > time::seconds::zero()) {
    reporter_thread_ = std::thread([=]() { PollForMetadata(); });
  }
}

void PollingMetadataUpdater::StopUpdater() {
  timer_.unlock();
  if (config().VerboseLogging()) {
    LOG(INFO) << "Timer unlocked";
  }
}

void PollingMetadataUpdater::PollForMetadata() {
  bool done = false;
  do {
    std::vector<ResourceMetadata> result_vector = query_metadata_();
    for (ResourceMetadata& result : result_vector) {
      UpdateResourceCallback(result);
      UpdateMetadataCallback(std::move(result));
    }
    // An unlocked timer means we should stop updating.
    if (config().VerboseLogging()) {
      LOG(INFO) << "Trying to unlock the timer";
    }
    auto start = std::chrono::high_resolution_clock::now();
    auto wakeup = start + period_;
    done = true;
    while (done && !timer_.try_lock_until(wakeup)) {
      auto now = std::chrono::high_resolution_clock::now();
      // Detect spurious wakeups.
      if (now < wakeup) {
        continue;
      }
      if (config().VerboseLogging()) {
        LOG(INFO) << " Timer unlock timed out after "
                  << std::chrono::duration_cast<time::seconds>(now - start).count()
                  << "s (good)";
      }
      start = now;
      wakeup = start + period_;
      done = false;
    }
  } while (!done);
  if (config().VerboseLogging()) {
    LOG(INFO) << "Timer unlocked (stop polling)";
  }
}

}
