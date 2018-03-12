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

} // namespace
