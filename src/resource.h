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
#ifndef RESOURCE_H_
#define RESOURCE_H_

#include <map>
#include <ostream>
#include <string>

#include "json.h"

namespace google {

class MonitoredResource {
 public:
  MonitoredResource(const std::string& type,
                    const std::map<std::string, std::string> labels)
      : type_(type), labels_(labels) {}
  const std::string& type() const { return type_; }
  const std::map<std::string, std::string>& labels() const { return labels_; }
  bool operator==(const MonitoredResource& other) const {
    return other.type_ == type_ && other.labels_ == labels_;
  }
  bool operator!=(const MonitoredResource& other) const {
    return !(other == *this);
  }
  bool operator<(const MonitoredResource& other) const {
    return type_ < other.type_
        || (type_ == other.type_ && labels_ < other.labels_);
  }

  json::value ToJSON() const;
  static MonitoredResource FromJSON(const json::Object* json);

 private:
  std::string type_;
  std::map<std::string, std::string> labels_;
};

std::ostream& operator<<(std::ostream&, const MonitoredResource&);

}

#endif  // RESOURCE_H_
