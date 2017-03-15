#ifndef OAUTH2_H_
#define OAUTH2_H_

#include <boost/network/message/directives/header.hpp>
#include <chrono>
#include <memory>
#include <string>

namespace google {

class OAuth2 {
 public:
  OAuth2() : OAuth2("") {}
  OAuth2(const std::string& credentials_file)
      : credentials_file_(credentials_file) {}

  std::string GetAuthHeaderValue();

  template<class Request>
  void AddAuthHeader(Request* request);

 private:
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
