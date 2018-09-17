#include "../src/reporter.h"

#include "../src/configuration.h"
#include "fake_clock.h"
#include "fake_http_server.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace google {

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
  // Set up a fake server representing the Resource Metadata API.  It
  // will collect POST data from the MetadataReporter.
  std::mutex mutex;
  std::condition_variable cv;
  int post_count = 0;
  std::map<std::string, std::string> last_headers;
  std::string last_body;
  testing::FakeServer server;
  constexpr const char kUpdatePath[] =
    "/v1beta2/projects/TestProjectId/resourceMetadata:batchUpdate";
  server.SetHandler(
      kUpdatePath,
      [&](const std::string& path,
          const std::map<std::string, std::string>& headers,
          const std::string& body) -> std::string {
        {
          std::lock_guard<std::mutex> lk(mutex);
          post_count++;
          last_headers = headers;
          last_body = body;
        }
        cv.notify_all();
        return "this POST response is ignored";
      });

  // Configure the the MetadataReporter to point to the fake server
  // and start it with a 60 second polling interval.  We control time
  // using a fake clock.
  Configuration config(std::istringstream(
      "ProjectId: TestProjectId\n"
      "MetadataIngestionEndpointFormat: " + server.GetUrl() +
      "/v1beta2/projects/{{project_id}}/resourceMetadata:batchUpdate\n"
  ));
  MetadataStore store(config);
  MonitoredResource resource("type", {});
  MetadataStore::Metadata m(
      "default-version",
      false,
      time::rfc3339::FromString("2018-03-03T01:23:45.678901234Z"),
      time::rfc3339::FromString("2018-03-03T01:32:45.678901234Z"),
      json::object({{"f", json::string("hello")}}));
  MetadataStore::Metadata m_deleted(
      "default-version",
      true,
      time::rfc3339::FromString("2018-03-03T01:23:45.678901234Z"),
      time::rfc3339::FromString("2018-03-03T01:32:45.678901234Z"),
      json::object({{"f", json::string("hello")}}));
  store.UpdateMetadata(resource, std::move(m));
  double period_s = 60.0;
  FakeMetadataReporter reporter(config, &store, period_s);

  // The headers & body we expect to see in the POST requests.
  std::pair<std::string, std::string> content_type(
      std::string("Content-Type"), std::string("application/json"));
  std::pair<std::string, std::string> user_agent(
      std::string("User-Agent"), std::string("metadata-agent/0.0.21-1"));
  auto expected_body = [](const std::string& state) -> std::string {
    return json::object({
      {"entries", json::array({
        json::object({  // MonitoredResourceMetadata
          {"resource", json::object({
              {"type", json::string("type")},
              {"labels", json::object({})},
          })},
          {"rawContentVersion", json::string("default-version")},
          {"rawContent", json::object({{"f", json::string("hello")}})},
          {"state", json::string(state)},
          {"createTime", json::string("2018-03-03T01:23:45.678901234Z")},
          {"collectTime", json::string("2018-03-03T01:32:45.678901234Z")},
        })
      })}
    })->ToString();
  };
  const std::string& expected_body_active = expected_body("ACTIVE");
  const std::string& expected_body_deleted = expected_body("DELETED");

  // Wait for 1st post to server, and verify contents.
  {
    std::unique_lock<std::mutex> lk(mutex);
    cv.wait(lk, [&post_count]{ return post_count >= 1; });
  }
  EXPECT_EQ(1, post_count);
  EXPECT_THAT(last_headers, ::testing::Contains(content_type));
  EXPECT_THAT(last_headers, ::testing::Contains(user_agent));
  EXPECT_EQ(expected_body_active, last_body);

  // Advance fake clock, wait for 2nd post, verify contents.
  testing::FakeClock::Advance(time::seconds(60));
  {
    std::unique_lock<std::mutex> lk(mutex);
    cv.wait(lk, [&post_count]{ return post_count >= 2; });
  }
  EXPECT_EQ(2, post_count);
  EXPECT_THAT(last_headers, ::testing::Contains(content_type));
  EXPECT_THAT(last_headers, ::testing::Contains(user_agent));
  EXPECT_EQ(expected_body_active, last_body);

  // Mark metadata as deleted in store, advance fake clock, wait for
  // 3rd post, verify contents.
  store.UpdateMetadata(resource, std::move(m_deleted));
  testing::FakeClock::Advance(time::seconds(60));
  {
    std::unique_lock<std::mutex> lk(mutex);
    cv.wait(lk, [&post_count]{ return post_count >= 3; });
  }
  EXPECT_EQ(3, post_count);
  EXPECT_THAT(last_headers, ::testing::Contains(content_type));
  EXPECT_THAT(last_headers, ::testing::Contains(user_agent));
  EXPECT_EQ(expected_body_deleted, last_body);

  reporter.NotifyStopReporter();
}

}  // namespace google
