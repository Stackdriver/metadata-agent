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
