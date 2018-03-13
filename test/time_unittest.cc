#include "../src/time.h"
#include "gtest/gtest.h"

using namespace google;

namespace {

TEST(TimeTest, EpochToString) {
  const time_point epoch;
  EXPECT_EQ(
      "1970-01-01T00:00:00.000000000Z",
      time::rfc3339::ToString(epoch)
  );
}

TEST(TimeTest, RoundtripViaTimePoint) {
  const time_point t =
      time::rfc3339::FromString("2018-03-03T01:23:45.678901234Z");
  EXPECT_EQ(
      "2018-03-03T01:23:45.678901234Z",
      time::rfc3339::ToString(t)
  );
}

TEST(TimeTest, RoundtripViaString) {
  const time_point t = std::chrono::system_clock::now();
  EXPECT_EQ(
      t,
      time::rfc3339::FromString(time::rfc3339::ToString(t))
  );
}

TEST(TimeTest, FromStringNoNanos) {
  const time_point t =
      time::rfc3339::FromString("2018-03-03T01:23:45Z");
  EXPECT_EQ(
      "2018-03-03T01:23:45.000000000Z",
      time::rfc3339::ToString(t)
  );
}

TEST(TimeTest, FromStringFewerDigits) {
  const time_point t =
      time::rfc3339::FromString("2018-03-03T01:23:45.6789Z");
  EXPECT_EQ(
      "2018-03-03T01:23:45.678900000Z",
      time::rfc3339::ToString(t)
  );
}

TEST(TimeTest, FromStringMoreDigits) {
  const time_point t =
      time::rfc3339::FromString("2018-03-03T01:23:45.67890123456789Z");
  EXPECT_EQ(
      "2018-03-03T01:23:45.678901234Z",
      time::rfc3339::ToString(t)
  );
}

TEST(TimeTest, FromStringLargeNanos) {
  const time_point t =
      time::rfc3339::FromString("2018-03-03T01:23:45.9876543210987Z");
  EXPECT_EQ(
      "2018-03-03T01:23:45.987654321Z",
      time::rfc3339::ToString(t)
  );
}

TEST(TimeTest, FromStringPositiveNanos) {
  const time_point t =
      time::rfc3339::FromString("2018-03-03T01:23:45.+678901234Z");
  EXPECT_EQ(
      "1970-01-01T00:00:00.000000000Z",
      time::rfc3339::ToString(t)
  );
}

TEST(TimeTest, FromStringNegativeNanos) {
  const time_point t =
      time::rfc3339::FromString("2018-03-03T01:23:45.-678901234Z");
  EXPECT_EQ(
      "1970-01-01T00:00:00.000000000Z",
      time::rfc3339::ToString(t)
  );
}

TEST(TimeTest, FromStringPositiveSeconds) {
  const time_point t =
      time::rfc3339::FromString("2018-03-03T01:23:+4.567890123Z");
  EXPECT_EQ(
      "1970-01-01T00:00:00.000000000Z",
      time::rfc3339::ToString(t)
  );
}

TEST(TimeTest, FromStringNegativeSeconds) {
  const time_point t =
      time::rfc3339::FromString("2018-03-03T01:23:-4.567890123Z");
  EXPECT_EQ(
      "1970-01-01T00:00:00.000000000Z",
      time::rfc3339::ToString(t)
  );
}

TEST(TimeTest, FromStringHexSeconds) {
  const time_point t =
      time::rfc3339::FromString("2018-03-03T01:23:0x45.678901234Z");
  EXPECT_EQ(
      "1970-01-01T00:00:00.000000000Z",
      time::rfc3339::ToString(t)
  );
}

TEST(TimeTest, FromStringInfSeconds) {
  const time_point t1 =
      time::rfc3339::FromString("2018-03-03T01:23:+infZ");
  EXPECT_EQ(
      "1970-01-01T00:00:00.000000000Z",
      time::rfc3339::ToString(t1)
  );
  const time_point t2 =
      time::rfc3339::FromString("2018-03-03T01:23:-infZ");
  EXPECT_EQ(
      "1970-01-01T00:00:00.000000000Z",
      time::rfc3339::ToString(t2)
  );
  const time_point t3 =
      time::rfc3339::FromString("2018-03-03T01:23:+nanZ");
  EXPECT_EQ(
      "1970-01-01T00:00:00.000000000Z",
      time::rfc3339::ToString(t3)
  );
  const time_point t4 =
      time::rfc3339::FromString("2018-03-03T01:23:-nanZ");
  EXPECT_EQ(
      "1970-01-01T00:00:00.000000000Z",
      time::rfc3339::ToString(t4)
  );
}

TEST(TimeTest, FromStringTooManySeconds) {
  const time_point t =
      time::rfc3339::FromString("2018-03-03T01:23:1045.678901234Z");
  EXPECT_EQ(
      "1970-01-01T00:00:00.000000000Z",
      time::rfc3339::ToString(t)
  );
}

TEST(TimeTest, FromStringScientificSeconds) {
  const time_point t =
      time::rfc3339::FromString("2018-03-03T01:23:45e+0Z");
  EXPECT_EQ(
      "1970-01-01T00:00:00.000000000Z",
      time::rfc3339::ToString(t)
  );
}

}
