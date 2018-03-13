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

#include "base64.h"

#include <vector>

namespace base64 {

namespace {
constexpr const unsigned char base64url_chars[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
}

// See http://stackoverflow.com/questions/180947.
std::string Encode(const std::string &source) {
  std::string result;
#if 0
  const std::size_t remainder = (source.size() - 1) % 3;
  for (std::size_t i = 0; i < source.size(); i += 3) {
    const bool one_more = i < source.size() - 3 || remainder > 0;
    const bool two_more = i < source.size() - 3 || remainder > 1;
    const unsigned char c0 = source[i];
    const unsigned char c1 = one_more ? source[i + 1] : 0u;
    const unsigned char c2 = two_more ? source[i + 2] : 0u;
    result.push_back(base64url_chars[0x3f & c0 >> 2]);
    result.push_back(base64url_chars[(0x3f & c0 << 4) | c1 >> 4]);
    if (one_more) {
      result.push_back(base64url_chars[(0x3f & c1 << 2) | c2 >> 6]);
    }
    if (two_more) {
      result.push_back(base64url_chars[0x3f & c2]);
    }
  }
#else
  unsigned int code_buffer = 0;
  int code_buffer_size = -6;
  for (unsigned char c : source) {
    code_buffer = (code_buffer << 8) | c;
    code_buffer_size += 8;
    while (code_buffer_size >= 0) {
      result.push_back(base64url_chars[(code_buffer >> code_buffer_size) & 0x3f]);
      code_buffer_size -= 6;
    }
  }
  if (code_buffer_size > -6) {
    code_buffer = (code_buffer << 8);
    code_buffer_size += 8;
    result.push_back(base64url_chars[(code_buffer >> code_buffer_size) & 0x3f]);
  }
#endif
  // No padding needed.
  return result;
}

std::string Decode(const std::string &source) {
  std::string result;

  std::vector<int> T(256, -1);
  for (int i = 0; i < 64; i++) T[base64url_chars[i]] = i;

  int val = 0, shift = -8;
  for (char c : source) {
    if (T[c] == -1) break;
    val = (val << 6) + T[c];
    shift += 6;
    if (shift >= 0) {
      result.push_back(char((val >> shift) & 0xFF));
      shift -= 8;
    }
  }
  return result;
}

}

