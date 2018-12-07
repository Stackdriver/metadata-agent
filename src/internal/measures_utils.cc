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

#include "measures_utils.h"

#include <opencensus/exporters/stats/prometheus/prometheus_exporter.h>
#include <prometheus/text_serializer.h>

namespace google {
namespace internal {

std::string SerializeMetricsToPrometheusTextFormat() {
  static const auto* const text_serializer =
      new ::prometheus::TextSerializer();
  static auto* const exporter =
      new ::opencensus::exporters::stats::PrometheusExporter();
  return text_serializer->Serialize(exporter->Collect());
}

} // namespace internal
} // namespace google
