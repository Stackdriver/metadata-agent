#include "../src/format.h"
#include "gtest/gtest.h"

namespace {

TEST(SubstituteTest, ValidFormat) {
  const std::string endpoint_format =
      "https://stackdriver.googleapis.com/v1beta2/projects/{{project_id}}";
  EXPECT_EQ(
      "https://stackdriver.googleapis.com/v1beta2/projects/my-project",
      format::Substitute(endpoint_format, {{"project_id", "my-project"}})
  );
}

TEST(SubstituteTest, ValidFormatMultipleSubstitution) {
  const std::string endpoint_format =
      "https://stackdriver.googleapis.com/{{version}}/projects/{{project_id}}";
  EXPECT_EQ(
      "https://stackdriver.googleapis.com/v1/projects/my-project",
      format::Substitute(endpoint_format, {
          {"project_id", "my-project"}, {"version", "v1"}})
  );
}

TEST(SubstituteTest, UnterminatedPlaceholder) {
  const std::string endpoint_format =
      "https://stackdriver.googleapis.com/v1beta1/projects/{{project_id";
  EXPECT_THROW(
      format::Substitute(endpoint_format, {{"project_id", "my-project"}}),
      format::Exception
  );
}

TEST(SubstituteTest, UnknownParameter) {
  const std::string endpoint_format =
      "https://stackdriver.googleapis.com/v1beta1/projects/{{project_id}}";
  EXPECT_THROW(
      format::Substitute(endpoint_format, {{"project", "my-project"}}),
      format::Exception
  );
}
} // namespace
