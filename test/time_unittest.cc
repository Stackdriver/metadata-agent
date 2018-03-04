#include "../src/time.h"
#include "gtest/gtest.h"

using namespace google;

namespace {

TEST(TimeTest, EpochToString) {
  const std::chrono::system_clock::time_point epoch;
  EXPECT_EQ(
      "1970-01-01T00:00:00.000000000Z",
      rfc3339::ToString(epoch)
  );
}

TEST(TimeTest, RoundtripViaTimePoint) {
  const std::chrono::system_clock::time_point t =
      rfc3339::FromString("2018-03-03T01:23:45.678901234Z");
  EXPECT_EQ(
      "2018-03-03T01:23:45.678901234Z",
      rfc3339::ToString(t)
  );
}

TEST(TimeTest, RoundtripViaString) {
  const std::chrono::system_clock::time_point t =
      std::chrono::system_clock::now();
  EXPECT_EQ(
      t,
      rfc3339::FromString(rfc3339::ToString(t))
  );
}

}
