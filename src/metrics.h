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
#include <string>

namespace google {

class Metrics {
 public:
  static const char kGceApiRequestErrors[];

  // Record an API request error on a method, e.g. oauth2 is one of the methods.
  static void RecordGceApiRequestErrors(int64_t value, const std::string& method);

  // Serialize all the available metrics as prometheus text format.
  static std::string SerializeMetricsToPrometheusTextFormat();

  // View Descriptor accessors. If the view descriptor variable is not
  // initialized, these methods will initialize the variable.
  static const ::opencensus::stats::ViewDescriptor
      GceApiRequestErrorsCumulativeViewDescriptor();

 private:
  static ::opencensus::stats::MeasureInt64 GceApiRequestErrorsInitialize();
  static ::opencensus::stats::MeasureInt64 GceApiRequestErrors();
};

}  // namespace google

#endif /* METADATA_AGENT_METRICS_H_ */
