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

#ifndef METADATA_AGENT_METRICS_H_
#define METADATA_AGENT_METRICS_H_

#include <opencensus/stats/stats.h>

namespace google {

class Metrics {
 public:
  // Register all the view descriptors declared above as view for export.
  static void RegisterAllViewsForExport();

  static void RecordGceApiRequestErrors(int64_t value, const std::string& method);

  // View Descriptor accessors. If the view descriptor variable is not
  // initialized, these methods will initialize the variable.
  static const ::opencensus::stats::ViewDescriptor& GceApiRequestErrorsCumulative();

 private:
  // Measure accessors. If the measure variable is not initialized, these methods
  // will initialize the variable.
  //
  // Reference of measure: https://opencensus.io/stats/measure/
  static ::opencensus::stats::MeasureInt64 GceApiRequestErrors();

  // Tag key accessors. If the tag key variable is not initialized, these methods
  // will initialize the variable.
  //
  // Reference of measure: https://opencensus.io/tag/key/
  static ::opencensus::stats::TagKey MethodTagKey();
};

} // namespace google

#endif /* METADATA_AGENT_METRICS_H_ */
