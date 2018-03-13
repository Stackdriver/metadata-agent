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
