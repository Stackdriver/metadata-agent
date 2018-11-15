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

#include <prometheus/registry.h>

namespace google {

class ApiServerTest : public ::testing::Test {
 protected:
  using Dispatcher = MetadataApiServer::Dispatcher;

  static std::string SerializeMetricsToPrometheusTextFormat(
      const google::MetadataApiServer& api_server) {
    return api_server.SerializeMetricsToPrometheusTextFormat();
  }

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

TEST_F(ApiServerTest, SerializationToPrometheusTextForNullPtr) {
  google::Configuration config;
  google::MetadataApiServer server(
    config,
    /*heapth_checker=*/nullptr,
    /*std::shared_ptr<prometheus::Collectable>=*/nullptr,
    MetadataStore(config),
    0, "", 8080);
  EXPECT_EQ("", SerializeMetricsToPrometheusTextFormat(server));
}

TEST_F(ApiServerTest, SerializationToPrometheusTextForEmptyRegistry) {
  google::Configuration config;
  std::shared_ptr<prometheus::Registry> registry;
  google::MetadataApiServer server(
    config, /*heapth_checker=*/nullptr, registry,
    MetadataStore(config),
    0, "", 8080);
  EXPECT_EQ("", SerializeMetricsToPrometheusTextFormat(server));
}

TEST_F(ApiServerTest, SerializationToPrometheusTextForNonEmptyRegistry) {
  google::Configuration config;
  auto registry = std::make_shared<prometheus::Registry>();
  prometheus::BuildCounter()
      .Name("test_metric_counter")
      .Help("help on test_metric_counter")
      .Register(*registry);
  std::string expected_result =
    "# HELP test_metric_counter help on test_metric_counter\n"
    "# TYPE test_metric_counter counter\n";
  google::MetadataApiServer server(
    config, /*heapth_checker=*/nullptr, registry,
    MetadataStore(config),
    0, "", 8080);
  EXPECT_EQ(expected_result, SerializeMetricsToPrometheusTextFormat(server));
}


TEST_F(ApiServerTest, SerializationToPrometheusTextWithLabeledCounter) {
  google::Configuration config;
  auto registry = std::make_shared<prometheus::Registry>();
  auto& counter_family = prometheus::BuildCounter()
      .Name("test_metric_counter")
      .Help("help on test_metric_counter")
      .Register(*registry);

  // Add a labeled counter
  counter_family.Add({{"foo", "bar"}});

  std::string expected_result =
    "# HELP test_metric_counter help on test_metric_counter\n"
    "# TYPE test_metric_counter counter\n"
    "test_metric_counter{foo=\"bar\"} 0.000000\n";
  google::MetadataApiServer server(
    config, /*heapth_checker=*/nullptr, registry,
    MetadataStore(config),
    0, "", 8080);
  EXPECT_EQ(expected_result, SerializeMetricsToPrometheusTextFormat(server));
}

}  // namespace google
