#include "../src/base64.h"
#include "gtest/gtest.h"

namespace {

TEST(EncodeTest, EmptyEncode) {
  const std::string empty_str("");
  EXPECT_EQ(
      "",
      base64::Encode(empty_str)
  );
}

TEST(EncodeTest, SimpleEncode) {
  const std::string simple_str("tes");
  EXPECT_EQ(
      "dGVz",
      base64::Encode(simple_str)
  );
}

TEST(EncodeTest, NoPaddingEncode) {
  const std::string simple_str("test0");
  EXPECT_EQ(
      "dGVzdDA",
      base64::Encode(simple_str)
  );
}

TEST(EncodeTest, NoDoublePaddingEncode) {
  const std::string simple_str("test");
  EXPECT_EQ(
      "dGVzdA",
      base64::Encode(simple_str)
  );
}

} // namespace
