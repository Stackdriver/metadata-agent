/*
 * Copyright 2018 Google Inc.
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

TEST(TimeTest, SecondsSinceEpoch) {
  const time_point t = time_point() + std::chrono::seconds(1500000000);
  EXPECT_EQ(1500000000, time::SecondsSinceEpoch(t));
}

TEST(TimeTest, SafeLocaltime) {
  const std::time_t now_c =
    std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
  std::tm local_time = time::safe_localtime(&now_c);
  EXPECT_EQ(std::mktime(std::localtime(&now_c)), std::mktime(&local_time));
}

TEST(TimeTest, SafeGmtime) {
  const std::time_t now_c =
    std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
  std::tm gm_time = time::safe_gmtime(&now_c);
  EXPECT_EQ(std::mktime(std::gmtime(&now_c)), std::mktime(&gm_time));
}

}
