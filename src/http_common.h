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
#ifndef HTTP_COMMON_H_
#define HTTP_COMMON_H_

#include <boost/network/protocol/http/client.hpp>
#include <boost/network/protocol/http/server.hpp>
#include <iostream>

namespace google {

// Allow logging request headers.
inline std::ostream& operator<<(
    std::ostream& o,
    const boost::network::http::basic_request<boost::network::http::tags::http_server>::headers_container_type& hv) {
  o << "[";
  for (const auto& h : hv) {
    o << " " << h.name << ": " << h.value;
  }
  o << " ]";
  return o;
}

// Allow logging response headers.
inline std::ostream& operator<<(
    std::ostream& o,
    const boost::network::http::basic_response<boost::network::http::BOOST_NETWORK_HTTP_CLIENT_DEFAULT_TAG>::headers_container_type& hv) {
  o << "[";
  for (const auto& h : hv) {
    o << " " << h.first << ": " << h.second;
  }
  o << " ]";
  return o;
}

}

#endif  // HTTP_COMMON_H_
