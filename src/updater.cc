#include "updater.h"

#include "logging.h"

#include <chrono>

namespace google {

PollingMetadataUpdater::PollingMetadataUpdater(
    double period_s, MetadataAgent* store,
    std::function<Metadata(void)> query_metadata)
    : period_(period_s),
      store_(store),
      query_metadata_(query_metadata),
      timer_(),
      reporter_thread_() {}

PollingMetadataUpdater::~PollingMetadataUpdater() {
  reporter_thread_.join();
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
  // An unlocked timer means we should stop updating.
  LOG(INFO) << "Trying to unlock the timer";
  auto start = std::chrono::high_resolution_clock::now();
  while (!timer_.try_lock_for(period_)) {
    auto now = std::chrono::high_resolution_clock::now();
    // Detect spurious wakeups.
    if (now - start < period_) continue;
    LOG(INFO) << " Timer unlock timed out after " << std::chrono::duration_cast<seconds>(now - start).count() << "s (good)";
    start = now;
    Metadata result = query_metadata_();
    store_->UpdateResource(result.id, result.resource, result.metadata);
  }
  LOG(INFO) << "Timer unlocked (stop polling)";
}

PollingMetadataUpdater::Metadata DockerMetadataQuery() {
  // TODO
  LOG(INFO) << "Docker Query called";
  return PollingMetadataUpdater::Metadata("", MonitoredResource("", {}), "");
}

}
