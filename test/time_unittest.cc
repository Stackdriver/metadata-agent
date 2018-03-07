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

TEST(TimeTest, FromStringNoNanos) {
  const std::chrono::system_clock::time_point t =
      rfc3339::FromString("2018-03-03T01:23:45Z");
  EXPECT_EQ(
      "2018-03-03T01:23:45.000000000Z",
      rfc3339::ToString(t)
  );
}

TEST(TimeTest, FromStringFewerDigits) {
  const std::chrono::system_clock::time_point t =
      rfc3339::FromString("2018-03-03T01:23:45.6789Z");
  EXPECT_EQ(
      "2018-03-03T01:23:45.678900000Z",
      rfc3339::ToString(t)
  );
}

TEST(TimeTest, FromStringMoreDigits) {
  const std::chrono::system_clock::time_point t =
      rfc3339::FromString("2018-03-03T01:23:45.67890123456789Z");
  EXPECT_EQ(
      "2018-03-03T01:23:45.678901234Z",
      rfc3339::ToString(t)
  );
}

TEST(TimeTest, FromStringLargeNanos) {
  const std::chrono::system_clock::time_point t =
      rfc3339::FromString("2018-03-03T01:23:45.9876543210987Z");
  EXPECT_EQ(
      "2018-03-03T01:23:45.987654321Z",
      rfc3339::ToString(t)
  );
}

TEST(TimeTest, FromStringPositiveNanos) {
  const std::chrono::system_clock::time_point t =
      rfc3339::FromString("2018-03-03T01:23:45.+678901234Z");
  EXPECT_EQ(
      "1970-01-01T00:00:00.000000000Z",
      rfc3339::ToString(t)
  );
}

TEST(TimeTest, FromStringNegativeNanos) {
  const std::chrono::system_clock::time_point t =
      rfc3339::FromString("2018-03-03T01:23:45.-678901234Z");
  EXPECT_EQ(
      "1970-01-01T00:00:00.000000000Z",
      rfc3339::ToString(t)
  );
}

TEST(TimeTest, FromStringPositiveSeconds) {
  const std::chrono::system_clock::time_point t =
      rfc3339::FromString("2018-03-03T01:23:+4.567890123Z");
  EXPECT_EQ(
      "1970-01-01T00:00:00.000000000Z",
      rfc3339::ToString(t)
  );
}

TEST(TimeTest, FromStringNegativeSeconds) {
  const std::chrono::system_clock::time_point t =
      rfc3339::FromString("2018-03-03T01:23:-4.567890123Z");
  EXPECT_EQ(
      "1970-01-01T00:00:00.000000000Z",
      rfc3339::ToString(t)
  );
}

TEST(TimeTest, FromStringHexSeconds) {
  const std::chrono::system_clock::time_point t =
      rfc3339::FromString("2018-03-03T01:23:0x45.678901234Z");
  EXPECT_EQ(
      "1970-01-01T00:00:00.000000000Z",
      rfc3339::ToString(t)
  );
}

TEST(TimeTest, FromStringInfSeconds) {
  const std::chrono::system_clock::time_point t1 =
      rfc3339::FromString("2018-03-03T01:23:+infZ");
  EXPECT_EQ(
      "1970-01-01T00:00:00.000000000Z",
      rfc3339::ToString(t1)
  );
  const std::chrono::system_clock::time_point t2 =
      rfc3339::FromString("2018-03-03T01:23:-infZ");
  EXPECT_EQ(
      "1970-01-01T00:00:00.000000000Z",
      rfc3339::ToString(t2)
  );
  const std::chrono::system_clock::time_point t3 =
      rfc3339::FromString("2018-03-03T01:23:+nanZ");
  EXPECT_EQ(
      "1970-01-01T00:00:00.000000000Z",
      rfc3339::ToString(t3)
  );
  const std::chrono::system_clock::time_point t4 =
      rfc3339::FromString("2018-03-03T01:23:-nanZ");
  EXPECT_EQ(
      "1970-01-01T00:00:00.000000000Z",
      rfc3339::ToString(t4)
  );
}

TEST(TimeTest, FromStringTooManySeconds) {
  const std::chrono::system_clock::time_point t =
      rfc3339::FromString("2018-03-03T01:23:1045.678901234Z");
  EXPECT_EQ(
      "1970-01-01T00:00:00.000000000Z",
      rfc3339::ToString(t)
  );
}

TEST(TimeTest, FromStringScientificSeconds) {
  const std::chrono::system_clock::time_point t =
      rfc3339::FromString("2018-03-03T01:23:45e+0Z");
  EXPECT_EQ(
      "1970-01-01T00:00:00.000000000Z",
      rfc3339::ToString(t)
  );
}

}
