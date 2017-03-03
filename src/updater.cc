#include "updater.h"

#include <chrono>
#include <iostream>

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
  std::cerr << std::hex << std::this_thread::get_id() << ": Timer locked" << std::endl;
  reporter_thread_ =
      std::thread(std::bind(&PollingMetadataUpdater::PollForMetadata, this));
}

void PollingMetadataUpdater::stop() {
  timer_.unlock();
  std::cerr << std::hex << std::this_thread::get_id() << ": Timer unlocked" << std::endl;
}

void PollingMetadataUpdater::PollForMetadata() {
  // An unlocked timer means we should stop updating.
  std::cerr << std::hex << std::this_thread::get_id() << ": Trying to unlock the timer" << std::endl;
  auto start = std::chrono::high_resolution_clock::now();
  while (!timer_.try_lock_for(period_)) {
    auto now = std::chrono::high_resolution_clock::now();
    // Detect spurious wakeups.
    if (now - start < period_) continue;
    std::cerr << std::hex << std::this_thread::get_id() << ": Timer unlock timed out after " << std::dec << std::chrono::duration_cast<seconds>(now - start).count() << "s (good)" << std::endl;
    start = now;
    Metadata result = query_metadata_();
    store_->UpdateResource(result.id, result.resource, result.metadata);
  }
  std::cerr << std::hex << std::this_thread::get_id() << ": Timer unlocked (stop polling)" << std::endl;
}

PollingMetadataUpdater::Metadata DockerMetadataQuery() {
  // TODO
  std::cerr << std::hex << std::this_thread::get_id() << ": Docker Query called" << std::endl;
  return PollingMetadataUpdater::Metadata("", MonitoredResource("", {}), "");
}

}
