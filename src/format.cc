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

#include "format.h"

#include <sstream>

namespace format {

namespace {
template<class T>
std::string AsString(T v) {
  std::ostringstream out;
  out << v;
  return out.str();
}
}

std::string str(int i) { return AsString(i); }
std::string str(double d) { return AsString(d); }
std::string str(bool b) { return AsString(b); }

std::string Substitute(const std::string& format,
                       const std::map<std::string, std::string>&& params)
    throw(Exception) {
  static constexpr const char kStart[] = "{{";
  static constexpr const std::size_t kStartLen = sizeof(kStart) - 1;
  static constexpr const char kEnd[] = "}}";
  static constexpr const std::size_t kEndLen = sizeof(kEnd) - 1;
  std::stringstream result;

  std::string::size_type pos = 0;
  for (std::string::size_type brace = format.find(kStart, pos);
       brace != std::string::npos;
       brace = format.find(kStart, pos)) {
    result << format.substr(pos, brace - pos);
    std::string::size_type open = brace + kStartLen;
    std::string::size_type close = format.find(kEnd, open);
    if (close == std::string::npos) {
      throw Exception("Unterminated placeholder at " + std::to_string(brace));
    }
    std::string param = format.substr(open, close - open);
    auto value_it = params.find(param);
    if (value_it == params.end()) {
      throw Exception(
          "Unknown parameter '" + param + "' at " + std::to_string(pos));
    }
    const std::string& value = value_it->second;
    result << value;
    pos = close + kEndLen;
  }
  result << format.substr(pos);
  return result.str();
}

}

