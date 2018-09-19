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

#include "../src/base64.h"
#include "gtest/gtest.h"

namespace {

TEST(EncodeTest, EmptyEncode) {
  EXPECT_EQ("", base64::Encode(""));
}

TEST(EncodeTest, SimpleEncode) {
  EXPECT_EQ("dGVz", base64::Encode("tes"));
}

// Base64 encoders typically pad messages to ensure output length % 4 == 0. To
// achieve this, encoders will pad messages with either one or two "=". Our
// implementation does not do this. The following two tests ensure that
// base64::Encode does not append one or two "=".
TEST(EncodeTest, OnePhantom) {
  EXPECT_EQ("dGVzdDA", base64::Encode("test0"));
}

TEST(EncodeTest, TwoPhantom) {
  EXPECT_EQ("dGVzdA", base64::Encode("test"));
}

TEST(DecodeTest, EmptyDecode) {
  EXPECT_EQ("", base64::Decode(""));
}

TEST(DecodeTest, SimpleDecode) {
  EXPECT_EQ("tes", base64::Decode("dGVz"));
}

TEST(DecodeTest, OnePadding) {
  EXPECT_EQ("test0", base64::Decode("dGVzdDA="));
}

TEST(DecodeTest, OnePhantom) {
  EXPECT_EQ("test0", base64::Decode("dGVzdDA"));
}

TEST(DecodeTest, TwoPadding) {
  EXPECT_EQ("test", base64::Decode("dGVzdA=="));
}

TEST(DecodeTest, TwoPhantom) {
  EXPECT_EQ("test", base64::Decode("dGVzdA"));
}

TEST(RoundTripTest, FullString) {
  EXPECT_EQ("tes", base64::Decode(base64::Encode("tes")));
}

TEST(RoundTripTest, OnePhantom) {
  EXPECT_EQ("test0", base64::Decode(base64::Encode("test0")));
}

TEST(RoundTripTest, TwoPhantoms) {
  EXPECT_EQ("test", base64::Decode(base64::Encode("test")));
}
} // namespace
