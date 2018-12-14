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

#include "../../src/metrics.h"

#include <opencensus/stats/stats.h>
#include <opencensus/stats/testing/test_utils.h>
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace google {

TEST(SerializeToPrometheusTextTest, NoMetricExists) {
  EXPECT_EQ("",
            ::google::Metrics::SerializeMetricsToPrometheusTextFormat());
}

TEST(SerializeToPrometheusTextTest, SingleMetricAndViewRegisteredForExport) {
  const char* measure_name = "test_measure";
  const char* view_name = "test_view";
  const ::opencensus::stats::MeasureInt64 test_measure =
      ::opencensus::stats::MeasureInt64::Register(
          measure_name, "description on test view", "1");
  const ::opencensus::stats::ViewDescriptor test_view_descriptor =
      ::opencensus::stats::ViewDescriptor()
          .set_name(view_name)
          .set_description("description on test view")
          .set_measure(measure_name)
          .set_aggregation(opencensus::stats::Aggregation::Count());
  // Remove view and flush the result before the actual test.
  ::opencensus::stats::StatsExporter::RemoveView(view_name);
  ::opencensus::stats::testing::TestUtils::Flush();

  test_view_descriptor.RegisterForExport();
  ::opencensus::stats::Record({{test_measure, 1}});
  // Flush to propagate existing records to views.
  ::opencensus::stats::testing::TestUtils::Flush();
  EXPECT_THAT(::google::Metrics::SerializeMetricsToPrometheusTextFormat(),
              ::testing::MatchesRegex(
                  "# HELP test_view_1 description on test view\n"
                  "# TYPE test_view_1 counter\n"
                  "test_view_1 1.000000 [0-9]*\n"));

  // Clean up the view which we registered for export.
  ::opencensus::stats::StatsExporter::RemoveView(view_name);
}

TEST(SerializeToPrometheusTextTest, RecordGceApiRequestErrors) {
  // Remove view and flush the result before the actual test.
  ::opencensus::stats::StatsExporter::RemoveView(::google::Metrics::kGceApiRequestErrors);
  ::opencensus::stats::testing::TestUtils::Flush();

  ::google::Metrics::RecordGceApiRequestErrors(1, "test_kind");
  // Flush to propagate existing records to views.
  ::opencensus::stats::testing::TestUtils::Flush();

  EXPECT_THAT(::google::Metrics::SerializeMetricsToPrometheusTextFormat(),
              ::testing::MatchesRegex(
                  "# HELP container_googleapis_com_internal_metadata_agent_gce_api_request_errors_1 The total number of HTTP request errors.\n"
                  "# TYPE container_googleapis_com_internal_metadata_agent_gce_api_request_errors_1 counter\n"
                  "container_googleapis_com_internal_metadata_agent_gce_api_request_errors_1\\{method=\"test_kind\"\\} 1.000000 [0-9]*\n"
              ));

  // Clean up the view which we registered for export.
  ::opencensus::stats::StatsExporter::RemoveView(::google::Metrics::kGceApiRequestErrors);
}

}  // namespace google
