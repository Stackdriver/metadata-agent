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
