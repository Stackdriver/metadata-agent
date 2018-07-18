#include "../src/reporter.h"

#include "../src/configuration.h"
#include "fake_clock.h"
#include "fake_http_server.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include <boost/algorithm/string/join.hpp>

namespace google {

namespace {
MATCHER_P(ContainsAll, map, "") {
  std::vector<std::string> missing;
  for (const auto& kv : map) {
    if (!::testing::Value(arg, ::testing::Contains(kv))) {
      missing.push_back(::testing::PrintToString(kv));
    }
  }
  if (!missing.empty()) {
    *result_listener << "does not contain "
                     << boost::algorithm::join(missing, " or ");
  }
  return missing.empty();
}
}  // namespace

// MetadataReporter that uses a FakeClock.
class FakeMetadataReporter : public MetadataReporter {
 public:
  FakeMetadataReporter(const Configuration& config,
                       MetadataStore* store, double period_s)
    : MetadataReporter(
          config, store, period_s, /*initial_wait_s=*/0,
          TimerImpl<testing::FakeClock>::New(false, "fake metadata reporter")) {}
};

TEST(ReporterTest, MetadataReporter) {
  // Set up a fake server representing the Resource Metadata API.
  // It will collect POST data from the MetadataReporter.
  std::mutex post_data_mutex;
  std::condition_variable post_data_cv;
  int post_count = 0;
  std::map<std::string, std::string> response_headers;
  std::string response_body;
  testing::FakeServer server;
  constexpr const char kUpdatePath[] =
    "/v1beta2/projects/TestProjectId/resourceMetadata:batchUpdate";
  server.SetHandler(
      kUpdatePath,
      [&](const std::string& path,
          const std::map<std::string, std::string>& headers,
          const std::string& body) -> std::string {
        {
          std::lock_guard<std::mutex> lk(post_data_mutex);
          post_count++;
          response_headers = headers;
          response_body = body;
        }
        post_data_cv.notify_all();
        return "this POST response is ignored";
      });

  // Configure the the MetadataReporter to point to the fake server
  // and start it with a 60 second polling interval.  We control time
  // using a fake clock.
  Configuration config(std::istringstream(
      "ProjectId: TestProjectId\n"
      "MetadataIngestionEndpointFormat: " + server.GetUrl() +
      "/v1beta2/projects/{{project_id}}/resourceMetadata:batchUpdate\n"
      "MetadataReporterUserAgent: metadata-agent/1.2.3-4\n"
  ));
  MetadataStore store(config);
  MonitoredResource resource("type", {});
  store.UpdateMetadata(MetadataStore::Metadata(
      "",
      "default-type",
      "default-location",
      "default-version",
      "default-schema",
      false,
      std::chrono::system_clock::now(),
      json::object({{"f", json::string("hello")}})));
  double period_s = 60.0;
  FakeMetadataReporter reporter(config, &store, period_s);

  // The headers and body we expect to see in the POST requests.
  std::map<std::string, std::string> expected_headers{
    {"Content-Type", "application/json"},
    {"User-Agent", "metadata-agent/1.2.3-4"},
  };

  json::value response;
  const json::Object* metadata_response;
  const json::Array* metadata_entries;
  const json::Object* metadata_entry;
  std::string state;

  // Wait for 1st post to server, and verify contents.
  {
    std::unique_lock<std::mutex> lk(post_data_mutex);
    post_data_cv.wait(lk, [&post_count]{ return post_count >= 1; });
  }
  EXPECT_EQ(1, post_count);
  EXPECT_THAT(response_headers, ContainsAll(expected_headers));
  response = json::Parser::FromString(response_body);
  metadata_response = response->As<json::Object>();
  metadata_entries = metadata_response->Get<json::Array>("entries");
  metadata_entry = (*metadata_entries)[0]->As<json::Object>();
  state = metadata_entry->Get<json::String>("state");
  EXPECT_EQ(state, "EXISTS");

  // Advance fake clock, wait for 2nd post, verify contents.
  testing::FakeClock::AdvanceAfterNextNowCall(time::seconds(60));
  {
    std::unique_lock<std::mutex> lk(post_data_mutex);
    post_data_cv.wait(lk, [&post_count]{ return post_count >= 2; });
  }
  EXPECT_EQ(2, post_count);
  EXPECT_THAT(response_headers, ContainsAll(expected_headers));
  response = json::Parser::FromString(response_body);
  metadata_response = response->As<json::Object>();
  metadata_entries = metadata_response->Get<json::Array>("entries");
  metadata_entry = (*metadata_entries)[0]->As<json::Object>();
  state = metadata_entry->Get<json::String>("state");
  EXPECT_EQ(state, "EXISTS");

  // Mark metadata as deleted in store, advance fake clock, wait for
  // 3rd post, verify contents.
  store.UpdateMetadata(MetadataStore::Metadata(
      "",
      "default-type",
      "default-location",
      "default-version",
      "default-schema",
      true,
      std::chrono::system_clock::now(),
      json::object({{"f", json::string("hello")}})));
  testing::FakeClock::AdvanceAfterNextNowCall(time::seconds(60));
  {
    std::unique_lock<std::mutex> lk(post_data_mutex);
    post_data_cv.wait(lk, [&post_count]{ return post_count >= 3; });
  }
  EXPECT_EQ(3, post_count);
  EXPECT_THAT(response_headers, ContainsAll(expected_headers));
  response = json::Parser::FromString(response_body);
  metadata_response = response->As<json::Object>();
  metadata_entries = metadata_response->Get<json::Array>("entries");
  metadata_entry = (*metadata_entries)[0]->As<json::Object>();
  state = metadata_entry->Get<json::String>("state");
  EXPECT_EQ(state, "DELETED");
}

}  // namespace google
