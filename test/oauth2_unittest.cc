/*
 * Copyright 2018 Google Inc.
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

#include "../src/oauth2.h"
#include "environment_util.h"
#include "fake_http_server.h"
#include "temp_file.h"
#include "gtest/gtest.h"

#include <sstream>

namespace google {

class OAuth2Test : public ::testing::Test {
 protected:
  static void SetTokenEndpointForTest(OAuth2* auth,
                                      const std::string& endpoint) {
    auth->SetTokenEndpointForTest(endpoint);
  }
};

namespace {

TEST_F(OAuth2Test, GetHeaderValueUsingTokenFromCredentials) {
  // TODO: Verify the POST body sent to the token endpoint.
  testing::FakeServer server;
  server.SetResponse("/oauth2/v3/token",
                     "{\"access_token\": \"the-access-token\","
                     " \"token_type\": \"Bearer\","
                     " \"expires_in\": 3600}");

  testing::TemporaryFile credentials_file(
    std::string(test_info_->name()) + "_creds.json",
    "{\"client_email\":\"user@example.com\",\"private_key\":\"some_key\"}");
  Configuration config(std::istringstream(
      "CredentialsFile: '" + credentials_file.FullPath().native() + "'\n"
  ));
  Environment environment(config);
  OAuth2 auth(environment);
  SetTokenEndpointForTest(&auth, server.GetUrl() + "/oauth2/v3/token");

  EXPECT_EQ("Bearer the-access-token", auth.GetAuthHeaderValue());
}

TEST_F(OAuth2Test, GetHeaderValueUsingTokenFromMetadataServer) {
  testing::FakeServer server;
  server.SetResponse("/instance/service-accounts/default/token",
                     "{\"access_token\": \"the-access-token\","
                     " \"token_type\": \"Bearer\","
                     " \"expires_in\": 3600}");

  Configuration config;
  Environment environment(config);
  testing::EnvironmentUtil::SetMetadataServerUrlForTest(
      &environment, server.GetUrl() + "/");

  OAuth2 auth(environment);
  EXPECT_EQ("Bearer the-access-token", auth.GetAuthHeaderValue());
}

}  // namespace
}  // namespace google
