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

#include "logging.h"

namespace google {

PollingMetadataUpdater::PollingMetadataUpdater(
    double period_s, MetadataAgent* store,
    std::function<std::vector<ResourceMetadata>()> query_metadata)
    : period_(period_s),
      store_(store),
      query_metadata_(query_metadata),
      timer_(),
      reporter_thread_() {}

PollingMetadataUpdater::~PollingMetadataUpdater() {
  if (reporter_thread_.joinable()) {
    reporter_thread_.join();
  }
}

void PollingMetadataUpdater::start() {
  timer_.lock();
  LOG(INFO) << "Timer locked";
  reporter_thread_ =
      std::thread(std::bind(&PollingMetadataUpdater::PollForMetadata, this));
}

void PollingMetadataUpdater::stop() {
  timer_.unlock();
  LOG(INFO) << "Timer unlocked";
}

void PollingMetadataUpdater::PollForMetadata() {
  bool done = false;
  do {
    std::vector<ResourceMetadata> result_vector = query_metadata_();
    for (ResourceMetadata& result : result_vector) {
      store_->UpdateResource(
          result.ids, result.resource, std::move(result.metadata));
    }
    // An unlocked timer means we should stop updating.
    LOG(INFO) << "Trying to unlock the timer";
    auto start = std::chrono::high_resolution_clock::now();
    done = true;
    while (!timer_.try_lock_for(period_)) {
      auto now = std::chrono::high_resolution_clock::now();
      // Detect spurious wakeups.
      if (now - start >= period_) {
        LOG(INFO) << " Timer unlock timed out after "
                  << std::chrono::duration_cast<seconds>(now - start).count()
                  << "s (good)";
        start = now;
        done = false;
        break;
      };
    }
  } while (!done);
  LOG(INFO) << "Timer unlocked (stop polling)";
}

}
