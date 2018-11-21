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

#include "measures.h"

#include <absl/strings/string_view.h>
#include <opencensus/stats/stats.h>

namespace google {

namespace {

constexpr char kUnitBytes[] = "By";
constexpr char kCount[] = "1";

} // namespace

ABSL_CONST_INIT const absl::string_view
    kGceApiRequestErrors =
        "container.googleapis.com/internal/metadata_agent/gce_api_request_errors";

::opencensus::stats::MeasureInt64 GceApiRequestErrors() {
  static const auto measure =
      ::opencensus::stats::MeasureInt64::Register(
          kGceApiRequestErrors,
          "Number of API request errors encountered.",
          kCount);
  return measure;
}

::opencensus::stats::TagKey MethodTagKey() {
  static const auto method_tag_key =
      ::opencensus::stats::TagKey::Register("method");
  return method_tag_key;
}

const ::opencensus::stats::ViewDescriptor& GceApiRequestErrorsCumulative() {
  const static ::opencensus::stats::ViewDescriptor descriptor =
      ::opencensus::stats::ViewDescriptor()
          .set_name("gce_api_request_errors")
          .set_measure(kGceApiRequestErrors)
          .set_aggregation(::opencensus::stats::Aggregation::Count())
          .add_column(MethodTagKey());
  return descriptor;
}

void RegisterAllViewsForExport() {
  GceApiRequestErrorsCumulative().RegisterForExport();
}

} // namespace google
