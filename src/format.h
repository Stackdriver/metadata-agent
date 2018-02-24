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
#ifndef FORMAT_H_
#define FORMAT_H_

#include <map>
#include <string>

namespace format {

// A representation of format substitution errors.
class Exception {
 public:
  Exception(const std::string& what) : explanation_(what) {}
  const std::string& what() const { return explanation_; }
 private:
  std::string explanation_;
};

std::string str(int);
std::string str(double);
std::string str(bool);

// Format string substitution.
// Placeholder format is '{{param}}'.
// All instances of each placeholder will be substituted by the value of the
// corresponding parameter.
std::string Substitute(const std::string& format,
                       const std::map<std::string, std::string>&& params)
    throw(Exception);

}  // format

#endif  // FORMAT_H_
