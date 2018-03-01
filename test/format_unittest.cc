#include "../src/format.h"
#include "gtest/gtest.h"

namespace {

TEST(SubstituteTest, SingleSubstitution) {
  const std::string format_str("prefix|{{subst}}|postfix");
  EXPECT_EQ(
      "prefix|value|postfix",
      format::Substitute(format_str, {{"subst", "value"}})
  );
}

TEST(SubstituteTest, MultipleSubstitutions) {
  const std::string format_str("prefix|{{subst}}|{{another_subst}}|postfix");
  EXPECT_EQ(
      "prefix|value|value2|postfix",
      format::Substitute(
          format_str, {{"subst", "value"}, {"another_subst", "value2"}})
  );
}

TEST(SubstituteTest, UnterminatedPlaceholder) {
  const std::string format_str("prefix|{{subst|postfix");
  EXPECT_THROW(
      format::Substitute(format_str, {{"subst", "value"}}),
      format::Exception
  );
}

TEST(SubstituteTest, UnknownParameter) {
  const std::string format_str("prefix|{{subst}}|postfix");
  EXPECT_THROW(
      format::Substitute(format_str, {{"another_subst", "value"}}),
      format::Exception
  );
}
} // namespace
