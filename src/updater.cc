#include "updater.h"

#include "json.h"
#include "local_stream_http.h"
#include "logging.h"

#include <boost/network/protocol/http/server.hpp>
#include <boost/network/protocol/http/client.hpp>
#include <chrono>

namespace http = boost::network::http;

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
  http::local_client client;
  http::local_client::request request("unix://%2Fvar%2Frun%2Fdocker.sock/v1.24/containers/json");
  http::local_client::response response = client.get(request);
  LOG(ERROR) << "Response: " << body(response);
  std::unique_ptr<json::Value> parsed = json::JSONParser::FromString(body(response));
  LOG(ERROR) << "Parsed response: " << *parsed;
  return PollingMetadataUpdater::Metadata("", MonitoredResource("", {}), "");
}

}
