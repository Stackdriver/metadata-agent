#include "../src/base64.h"
#include "gtest/gtest.h"

namespace {

TEST(EncodeTest, EmptyEncode) {
  EXPECT_EQ(
      "",
      base64::Encode("")
  );
}

TEST(EncodeTest, SimpleEncode) {
  EXPECT_EQ(
      "dGVz",
      base64::Encode("tes")
  );
}

//Base64 encodings typically pad messages to ensure output length % 4 == 0, our
//implementation does not
TEST(EncodeTest, NoPaddingEncode) {
  EXPECT_EQ(
      "dGVzdDA",
      base64::Encode("test0")
  );
}

TEST(EncodeTest, NoDoublePaddingEncode) {
  EXPECT_EQ(
      "dGVzdA",
      base64::Encode("test")
  );
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
