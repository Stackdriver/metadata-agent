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
#include "fake_clock.h"
#include "fake_http_server.h"
#include "temp_file.h"
#include "gmock/gmock.h"
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

TEST_F(OAuth2Test, GetAuthHeaderValueUsingTokenFromCredentials) {
  testing::FakeServer oauth_server;
  oauth_server.SetPostResponse("/oauth2/v3/token",
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
  SetTokenEndpointForTest(&auth, oauth_server.GetUrl() + "/oauth2/v3/token");

  EXPECT_EQ("Bearer the-access-token", auth.GetAuthHeaderValue());

  // Verify the POST contents sent to the token endpoint.
  const auto& posts = oauth_server.GetPosts("/oauth2/v3/token");
  EXPECT_EQ(1, posts.size());

  const auto& post = posts[0];
  EXPECT_EQ("application/x-www-form-urlencoded",
            post.headers.at("Content-Type"));
  EXPECT_THAT(post.body, ::testing::StartsWith(
      "grant_type=urn%3Aietf%3Aparams%3Aoauth%3Agrant-type%3Ajwt-bearer"
      "&assertion="));
}

TEST_F(OAuth2Test, GetAuthHeaderValueUsingTokenFromMetadataServer) {
  testing::FakeServer metadata_server;
  metadata_server.SetResponse("/instance/service-accounts/default/token",
                              "{\"access_token\": \"the-access-token\","
                              " \"token_type\": \"Bearer\","
                              " \"expires_in\": 3600}");
  Configuration config;
  Environment environment(config);
  testing::EnvironmentUtil::SetMetadataServerUrlForTest(
      &environment, metadata_server.GetUrl() + "/");
  OAuth2 auth(environment);

  EXPECT_EQ("Bearer the-access-token", auth.GetAuthHeaderValue());
}

TEST_F(OAuth2Test, GetAuthHeaderValueUsingTokenFromMetadataServerAsFallback) {
  testing::FakeServer metadata_server;
  metadata_server.SetResponse("/instance/service-accounts/default/token",
                              "{\"access_token\": \"the-access-token\","
                              " \"token_type\": \"Bearer\","
                              " \"expires_in\": 3600}");
  // Setup fake OAuth endpoint, but don't set response for
  // /oauth2/v3/token, so that it returns an error.
  //
  // We'll check that we fallback to the Metadata Server.
  testing::FakeServer oauth_server;
  testing::TemporaryFile credentials_file(
    std::string(test_info_->name()) + "_creds.json",
    "{\"client_email\":\"user@example.com\",\"private_key\":\"some_key\"}");
  Configuration config(std::istringstream(
      "CredentialsFile: '" + credentials_file.FullPath().native() + "'\n"
  ));
  Environment environment(config);
  testing::EnvironmentUtil::SetMetadataServerUrlForTest(
      &environment, metadata_server.GetUrl() + "/");
  OAuth2 auth(environment);
  SetTokenEndpointForTest(&auth, oauth_server.GetUrl() + "/oauth2/v3/token");

  EXPECT_EQ("Bearer the-access-token", auth.GetAuthHeaderValue());
}

TEST_F(OAuth2Test, GetAuthHeaderValueTokenJsonMissingField) {
  testing::FakeServer metadata_server;
  // JSON is missing "expires_in" field.
  metadata_server.SetResponse("/instance/service-accounts/default/token",
                              "{\"access_token\": \"the-access-token\","
                              " \"token_type\": \"Bearer\"}");
  Configuration config;
  Environment environment(config);
  testing::EnvironmentUtil::SetMetadataServerUrlForTest(
      &environment, metadata_server.GetUrl() + "/");
  OAuth2 auth(environment);

  EXPECT_EQ("", auth.GetAuthHeaderValue());
}

TEST_F(OAuth2Test, GetAuthHeaderValueMetadataServerReturnsEmptyToken) {
  testing::FakeServer metadata_server;
  metadata_server.SetResponse("/instance/service-accounts/default/token", "");
  Configuration config;
  Environment environment(config);
  testing::EnvironmentUtil::SetMetadataServerUrlForTest(
      &environment, metadata_server.GetUrl() + "/");
  OAuth2 auth(environment);

  EXPECT_EQ("", auth.GetAuthHeaderValue());
}

// OAuth2 implementation using a FakeClock for token expiration.
class FakeOAuth2 : public OAuth2 {
 public:
  FakeOAuth2(const Environment& environment)
    : OAuth2(environment,
             std::unique_ptr<Expiration>(
                 new ExpirationImpl<testing::FakeClock>())) {}
};

TEST_F(OAuth2Test, GetAuthHeaderValueCachingAndExpiration) {
  testing::FakeServer metadata_server;
  Configuration config;
  Environment environment(config);
  testing::EnvironmentUtil::SetMetadataServerUrlForTest(
      &environment, metadata_server.GetUrl() + "/");
  FakeOAuth2 auth(environment);

  // Metadata Server returns token "1".
  metadata_server.SetResponse("/instance/service-accounts/default/token",
                              "{\"access_token\": \"the-access-token-1\","
                              " \"token_type\": \"Bearer\","
                              " \"expires_in\": 3600}");
  EXPECT_EQ("Bearer the-access-token-1", auth.GetAuthHeaderValue());

  // Metadata Server returns token "2", but cached token is still "1".
  metadata_server.SetResponse("/instance/service-accounts/default/token",
                              "{\"access_token\": \"the-access-token-2\","
                              " \"token_type\": \"Bearer\","
                              " \"expires_in\": 3600}");
  EXPECT_EQ("Bearer the-access-token-1", auth.GetAuthHeaderValue());

  // Advance clock only 2000, still use cached token of "1".
  testing::FakeClock::Advance(std::chrono::seconds(2000));
  EXPECT_EQ("Bearer the-access-token-1", auth.GetAuthHeaderValue());

  // Advance clock another 1550, so now it is within 60 seconds of
  // expiration and we fetch new token of "2" from Metadata Server.
  testing::FakeClock::Advance(std::chrono::seconds(1550));
  EXPECT_EQ("Bearer the-access-token-2", auth.GetAuthHeaderValue());
}

}  // namespace
}  // namespace google
