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

#include "../src/api_server.h"
#include "../src/configuration.h"
#include "../src/store.h"
#include "gtest/gtest.h"

namespace google {

class ApiServerTest : public ::testing::Test {
 protected:
  using Dispatcher = MetadataApiServer::Dispatcher;
  std::unique_ptr<Dispatcher> CreateDispatcher(
      const std::map<std::pair<std::string, std::string>,
                     std::function<void()>>& handlers) {
    Dispatcher::HandlerMap handler_map;
    for (const auto& element : handlers) {
      std::function<void()> handler = element.second;
      handler_map.emplace(element.first, [handler](
          const MetadataApiServer::HttpServer::request& request,
          std::shared_ptr<MetadataApiServer::HttpServer::connection> conn) {
        handler();
      });
    }
    return std::unique_ptr<Dispatcher>(new Dispatcher(handler_map, false));
  }

  void InvokeDispatcher(
      const std::unique_ptr<Dispatcher>& dispatcher,
      const std::string& method, const std::string& path) {
    MetadataApiServer::HttpServer::request request;
    request.method = method;
    request.destination = path;
    (*dispatcher)(request, nullptr);
  }
};

TEST_F(ApiServerTest, DispatcherMatchesFullPath) {
  bool handler_called = false;
  bool bad_handler_called = false;
  std::unique_ptr<Dispatcher> dispatcher = CreateDispatcher({
    {{"GET", "/testPath/"}, [&handler_called]() {
      handler_called = true;
    }},
    {{"GET", "/badPath/"}, [&bad_handler_called]() {
      bad_handler_called = true;
    }},
  });

  InvokeDispatcher(dispatcher, "GET", "/testPath/");
  EXPECT_TRUE(handler_called);
  EXPECT_FALSE(bad_handler_called);
}

TEST_F(ApiServerTest, DispatcherUnmatchedMethod) {
  bool handler_called = false;
  std::unique_ptr<Dispatcher> dispatcher = CreateDispatcher({
    {{"GET", "/testPath/"}, [&handler_called]() {
      handler_called = true;
    }},
  });

  InvokeDispatcher(dispatcher, "POST", "/testPath/");
  EXPECT_FALSE(handler_called);
}

TEST_F(ApiServerTest, DispatcherSubstringCheck) {
  bool handler_called = false;
  std::unique_ptr<Dispatcher> dispatcher = CreateDispatcher({
    {{"GET", "/testPath/"}, [&handler_called]() {
      handler_called = true;
    }},
  });

  InvokeDispatcher(dispatcher, "GET", "/testPathFoo/");
  EXPECT_FALSE(handler_called);
  InvokeDispatcher(dispatcher, "GET", "/test/");
  EXPECT_FALSE(handler_called);
  InvokeDispatcher(dispatcher, "GET", "/testFooPath/");
  EXPECT_FALSE(handler_called);
  InvokeDispatcher(dispatcher, "GET", "/testPath/subPath/");
  EXPECT_TRUE(handler_called);
}
}  // namespace google
