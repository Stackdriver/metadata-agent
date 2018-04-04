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

   static void BasicDispatcher() {
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
      MetadataApiServer::Dispatcher::HandlerMap handlers({
        {{"GET", "/testPath/"}, handler},
        {{"GET", "/badPath/"}, badHandler},
      });
      MetadataApiServer::Dispatcher dispatcher(handlers, false);
      MetadataApiServer::HttpServer::request request;
      request.method = "GET";
      request.destination = "/testPath/";
      dispatcher(request, nullptr);
      EXPECT_TRUE(handler_called);
   }

   static void DispatcherMethodCheck() {
      bool handler_called;
      MetadataApiServer::Handler handler = [&handler_called](
          const MetadataApiServer::HttpServer::request& request,
          std::shared_ptr<MetadataApiServer::HttpServer::connection> conn) {
        handler_called = true;
      };
      MetadataApiServer::Dispatcher::HandlerMap handlers({
        {{"GET", "/testPath/"}, handler},
      });
      MetadataApiServer::Dispatcher dispatcher(handlers, false);
      MetadataApiServer::HttpServer::request request;
      request.method = "POST";
      request.destination = "/testPath/";
      dispatcher(request, nullptr);
      EXPECT_FALSE(handler_called);
   }

   static void DispatcherSubstringCheck() {
      bool handler_called;
      MetadataApiServer::Handler handler = [&handler_called](
          const MetadataApiServer::HttpServer::request& request,
          std::shared_ptr<MetadataApiServer::HttpServer::connection> conn) {
        handler_called = true;
      };
      MetadataApiServer::Dispatcher::HandlerMap handlers({
        {{"GET", "/testPath/"}, handler},
      });
      MetadataApiServer::Dispatcher dispatcher(handlers, false);
      MetadataApiServer::HttpServer::request request;
      request.method = "GET";
      request.destination = "/testPathFoo/";
      dispatcher(request, nullptr);
      EXPECT_FALSE(handler_called);
      request.destination = "/test/";
      dispatcher(request, nullptr);
      EXPECT_FALSE(handler_called);
      request.destination = "/testFooPath/";
      dispatcher(request, nullptr);
      EXPECT_FALSE(handler_called);
   }
};

TEST_F(ApiServerTest, BasicDispacher) {
  BasicDispatcher();
}

TEST_F(ApiServerTest, DispatcherMethodCheck) {
  DispatcherMethodCheck();
}

TEST_F(ApiServerTest, DispatcherSubstringCheck) {
  DispatcherSubstringCheck();
}

}  // namespace google
