#ifndef OAUTH2_H_
#define OAUTH2_H_

#include <boost/network/message/directives/header.hpp>
#include <chrono>
#include <memory>
#include <string>

#include "environment.h"
#include "json.h"

namespace google {

class OAuth2 {
 public:
  OAuth2(const Environment& environment)
      : environment_(environment),
        credentials_file_(environment.config_.CredentialsFile()) {}

  std::string GetAuthHeaderValue();

  template<class Request>
  void AddAuthHeader(Request* request);

 private:
  json::value ComputeToken(const std::string& credentials_file) const;
  json::value GetMetadataToken() const;

  const Environment& environment_;
  std::string credentials_file_;
  std::string auth_header_value_;
  std::chrono::time_point<std::chrono::system_clock> token_expiration_;
};

template<class Request>
void OAuth2::AddAuthHeader(Request* request) {
  (*request) << boost::network::header("Authorization", GetAuthHeaderValue());
}

}

#endif  // OAUTH2_H_
