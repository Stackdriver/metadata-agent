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

#include "metrics.h"

#include <opencensus/exporters/stats/prometheus/prometheus_exporter.h>
#include <opencensus/stats/stats.h>
#include <prometheus/text_serializer.h>

namespace google {

const char Metrics::kGceApiRequestErrors[] =
    "container.googleapis.com/internal/metadata_agent/gce_api_request_errors";

namespace {

constexpr char kCount[] = "1";

::opencensus::stats::TagKey MethodTagKey() {
  static const auto method_tag_key =
      ::opencensus::stats::TagKey::Register("method");
  return method_tag_key;
}

} // namespace


void Metrics::RecordGceApiRequestErrors(int64_t value,
                                        const std::string& method) {
    ::opencensus::stats::Record(
      {{GceApiRequestErrors(), value}},
      {{MethodTagKey(), method}});
}

const ::opencensus::stats::ViewDescriptor
    Metrics::GceApiRequestErrorsCumulative() {
  return ::opencensus::stats::ViewDescriptor()
      .set_name(kGceApiRequestErrors)
      .set_measure(kGceApiRequestErrors)
      .set_aggregation(::opencensus::stats::Aggregation::Count())
      .add_column(MethodTagKey());
}

::opencensus::stats::MeasureInt64 Metrics::GceApiRequestErrors() {
  static const auto measure = Metrics::GceApiRequestErrorsInitialize();
  return measure;
}

::opencensus::stats::MeasureInt64 Metrics::GceApiRequestErrorsInitialize() {
  auto measure =
      ::opencensus::stats::MeasureInt64::Register(
          kGceApiRequestErrors,
          "Number of API request errors encountered.",
          kCount);
  Metrics::GceApiRequestErrorsCumulative().RegisterForExport();
  return measure;
}

::opencensus::stats::ViewData::DataMap<int64_t>
    Metrics::GetGceApiRequestErrorsCumulativeViewIntData() {
  static ::opencensus::stats::View errors_view(
      GceApiRequestErrorsCumulative());
  return errors_view.GetData().int_data();
}

std::string Metrics::SerializeMetricsToPrometheusTextFormat() {
  return ::prometheus::TextSerializer().Serialize(
      ::opencensus::exporters::stats::PrometheusExporter().Collect());
}

} // namespace google
