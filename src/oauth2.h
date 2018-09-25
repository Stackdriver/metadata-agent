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
#ifndef OAUTH2_H_
#define OAUTH2_H_

#include <boost/network/message/directives/header.hpp>
#include <memory>
#include <string>

#include "environment.h"
#include "json.h"
#include "time.h"

namespace google {

namespace {
constexpr const char kDefaultTokenEndpoint[] =
  "https://www.googleapis.com/oauth2/v3/token";
}

class Expiration {
 public:
  virtual ~Expiration() = default;
  virtual bool IsExpired() = 0;
  virtual void Reset(std::chrono::seconds duration) = 0;
};

template<typename Clock>
class ExpirationImpl : public Expiration {
 public:
  static std::unique_ptr<Expiration> New(std::chrono::seconds slack) {
    return std::unique_ptr<Expiration>(new ExpirationImpl<Clock>(slack));
  }

  explicit ExpirationImpl(std::chrono::seconds slack) : slack_(slack) {}

  bool IsExpired() override {
    return token_expiration_ < Clock::now() + slack_;
  }

  void Reset(std::chrono::seconds duration) override {
    token_expiration_ = Clock::now() + duration;
  }

 private:
  typename Clock::time_point token_expiration_;
  std::chrono::seconds slack_;
};

class OAuth2 {
 public:
  OAuth2(const Environment& environment)
    : OAuth2(environment,
             // Build in a 60 second slack to avoid timing problems
             // (clock skew, races).
             ExpirationImpl<std::chrono::system_clock>::New(
                 std::chrono::seconds(60))) {}

  std::string GetAuthHeaderValue();

 protected:
  OAuth2(const Environment& environment, std::unique_ptr<Expiration> expiration)
    : environment_(environment),
      token_expiration_(std::move(expiration)),
      token_endpoint_(kDefaultTokenEndpoint) {}

 private:
  friend class OAuth2Test;

  json::value ComputeTokenFromCredentials() const;
  json::value GetMetadataToken() const;

  void SetTokenEndpointForTest(const std::string& endpoint) {
    token_endpoint_ = endpoint;
  }

  const Environment& environment_;
  std::string auth_header_value_;
  std::unique_ptr<Expiration> token_expiration_;
  std::string token_endpoint_;
};

}

#endif  // OAUTH2_H_
