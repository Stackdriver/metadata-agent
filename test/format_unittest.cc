#include "../src/format.h"
#include "gtest/gtest.h"

namespace {

TEST(SubstituteTest, SingleSubstitution) {
  const std::string format_str("prefix|{{subst}}|suffix");
  EXPECT_EQ(
      "prefix|value|suffix",
      format::Substitute(format_str, {{"subst", "value"}})
  );
}

TEST(SubstituteTest, MultipleSubstitutions) {
  const std::string format_str("prefix|{{first}}|middle|{{second}}|suffix");
  EXPECT_EQ(
      "prefix|one|middle|two|suffix",
      format::Substitute(
          format_str, {{"first", "one"}, {"second", "two"}})
  );
}

TEST(SubstituteTest, UnterminatedPlaceholder) {
  const std::string format_str("prefix|{{subst|suffix");
  EXPECT_THROW(
      format::Substitute(format_str, {{"subst", "value"}}),
      format::Exception
  );
}

TEST(SubstituteTest, UnknownParameter) {
  const std::string format_str("prefix|{{subst}}|suffix");
  EXPECT_THROW(
      format::Substitute(format_str, {{"unknown", "value"}}),
      format::Exception
  );
}
} // namespace
