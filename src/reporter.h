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
#ifndef REPORTER_H_
#define REPORTER_H_

#include <boost/system/system_error.hpp>
#include <map>
#include <string>
#include <thread>

#include "environment.h"
#include "json.h"
#include "oauth2.h"
#include "resource.h"
#include "store.h"
#include "time.h"

namespace google {

// Configuration object.
class Configuration;

// A periodic reporter of metadata to Stackdriver.
class MetadataReporter {
 public:
  MetadataReporter(const Configuration& config,
                   MetadataStore* store, double period_s);
  ~MetadataReporter();

 private:
  // Metadata reporter.
  void ReportMetadata();

  // Send the given set of metadata.
  void SendMetadata(
      std::map<MonitoredResource, MetadataStore::Metadata>&& metadata)
      throw (boost::system::system_error);

  const Configuration& config_;
  MetadataStore* store_;
  Environment environment_;
  OAuth2 auth_;
  // The reporting period in seconds.
  time::seconds period_;
  std::thread reporter_thread_;
};

}

#endif  // REPORTER_H_
