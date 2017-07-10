/*
 * Copyright 2017 Google Inc.
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

#include "resource.h"

#include <sstream>

namespace google {

std::ostream& operator<<(std::ostream& o, const MonitoredResource& r) {
  o << "{ type: '" << r.type() << "' labels: {";
  for (const auto& label : r.labels()) {
    o << " " << label.first << ": '" << label.second << "'";
  }
  o << " } }";
}

json::value MonitoredResource::ToJSON() const {
  std::map<std::string, json::value> labels;
  for (const auto& kv : labels_) {
    labels.emplace(kv.first, json::string(kv.second));
  }
  return json::object({
    {"type", json::string(type_)},
    {"labels", json::object(std::move(labels))},
  });
}

}
