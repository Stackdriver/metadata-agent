#include "../src/api_server.h"
#include "../src/configuration.h"
#include "../src/store.h"
#include "gtest/gtest.h"

namespace google {

class ApiServerTest : public ::testing::Test {
  protected:
   static void CallDispatcher(
       MetadataApiServer* server,
       const MetadataApiServer::HttpServer::request& request,
       std::shared_ptr<MetadataApiServer::HttpServer::connection> conn) {
    server->dispatcher_(request, conn);
   }
};

TEST_F(ApiServerTest, BasicDispacher) {
  Configuration config(std::istringstream(""));
  MetadataStore store(config);
  bool handler_called;
  MetadataApiServer::Handler handler = [&handler_called](
      const MetadataApiServer::HttpServer::request& request,
      std::shared_ptr<MetadataApiServer::HttpServer::connection> conn) {
    handler_called = true;
  };
  MetadataApiServer::Handler badHandler = [&handler_called](
      const MetadataApiServer::HttpServer::request& request,
      std::shared_ptr<MetadataApiServer::HttpServer::connection> conn) {
    handler_called = false;
  };
  MetadataApiServer::HandlerMap handlers({
    {{"GET", "/testPath/"}, handler},
    {{"GET", "/badPath/"}, badHandler}
  });
  MetadataApiServer server(config, store, 0, "TestHost", 1234, handlers);
  MetadataApiServer::HttpServer::request request;
  request.method = "GET";
  request.destination = "/testPath/";
  CallDispatcher(&server, request, nullptr);
  EXPECT_TRUE(handler_called);
}

TEST_F(ApiServerTest, DispatcherMethodCheck) {
  Configuration config(std::istringstream(""));
  MetadataStore store(config);
  bool handler_called;
  MetadataApiServer::Handler handler = [&handler_called](
      const MetadataApiServer::HttpServer::request& request,
      std::shared_ptr<MetadataApiServer::HttpServer::connection> conn) {
    handler_called = true;
  };
  MetadataApiServer::HandlerMap handlers({{{"GET", "/testPath/"}, handler}});
  MetadataApiServer server(config, store, 0, "TestHost", 1234, handlers);
  MetadataApiServer::HttpServer::request request;
  request.method = "POST";
  request.destination = "/testPath/";
  CallDispatcher(&server, request, nullptr);
  EXPECT_FALSE(handler_called);
}

TEST_F(ApiServerTest, DispatcherSubstringCheck) {
  Configuration config(std::istringstream(""));
  MetadataStore store(config);
  bool handler_called;
  MetadataApiServer::Handler handler = [&handler_called](
      const MetadataApiServer::HttpServer::request& request,
      std::shared_ptr<MetadataApiServer::HttpServer::connection> conn) {
    handler_called = true;
  };
  MetadataApiServer::HandlerMap handlers({{{"GET", "/testPath/"}, handler}});
  MetadataApiServer server(config, store, 0, "TestHost", 1234, handlers);
  MetadataApiServer::HttpServer::request request;

  request.method = "GET";
  request.destination = "/testPathFoo/";
  CallDispatcher(&server, request, nullptr);
  EXPECT_FALSE(handler_called);
  request.destination = "/test/";
  CallDispatcher(&server, request, nullptr);
  EXPECT_FALSE(handler_called);
  request.destination = "/testFooPath/";
  CallDispatcher(&server, request, nullptr);
  EXPECT_FALSE(handler_called);
}

}  // namespace google
