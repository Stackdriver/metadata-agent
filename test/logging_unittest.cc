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

#include "../src/logging.h"
#include "gtest/gtest.h"

#include <regex>
#include <sstream>

namespace google {
namespace {

TEST(LoggingTest, Logger) {
  std::ostringstream out;

  // Logger flushes on destruction.
  {
    LogStream stream(out);
    Logger logger("somefile.cc", 123, Logger::WARNING, &stream);
    logger << "Test message";
  }

  EXPECT_TRUE(std::regex_match(out.str(), std::regex(
     "W\\d{4} \\d{2}:\\d{2}:\\d{2} .+ "
     "somefile.cc:123 Test message\n")));
}

}  // namespace
}  // namespace google
