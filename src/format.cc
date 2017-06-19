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

std::string Substitute(const std::string& format,
                       const std::map<std::string, std::string>&& params)
    throw(Exception) {
  static constexpr const char kStart[] = "{{";
  static constexpr const char kEnd[] = "}}";
  std::stringstream result;

  std::string::size_type pos = 0;
  for (std::string::size_type brace = format.find(kStart, pos);
       brace != std::string::npos;
       brace = format.find(kStart, pos)) {
    result << format.substr(pos, brace - pos);
    std::string::size_type open = brace + 2;
    std::string::size_type close = format.find(kEnd, open);
    if (close == std::string::npos) {
      throw Exception("Unterminated placeholder at " + std::to_string(pos));
    }
    std::string param = format.substr(open, close - open);
    auto value_it = params.find(param);
    if (value_it == params.end()) {
      throw Exception(
          "Unknown parameter '" + param + "' at " + std::to_string(pos));
    }
    const std::string& value = value_it->second;
    result << value;
    pos = close + 2;
  }
  result << format.substr(pos);
  return result.str();
}

}

