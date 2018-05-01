#include "../src/util.h"
#include "gtest/gtest.h"

#include "../src/time.h"

namespace google {

namespace {

class Fail {};

class Terminate {};

class TerminateSubclass : public Fail {};

TEST(RetryTest, NoRetriesOnSuccess) {
  int invocation_count = 0;
  EXPECT_NO_THROW((util::Retry<Terminate, Fail>(
      10, time::seconds(0.01),
      [&invocation_count]() { ++invocation_count; })));
  EXPECT_EQ(1, invocation_count);
}

TEST(RetryTest, RetryOnFail) {
  int invocation_count = 0;
  EXPECT_THROW(
      (util::Retry<Terminate, Fail>(
           10, time::seconds(0.01),
           [&invocation_count]() { ++invocation_count; throw Fail(); })),
      Fail);
  EXPECT_EQ(10, invocation_count);
}

TEST(RetryTest, NoRetryOnTerminate) {
  int invocation_count = 0;
  EXPECT_THROW(
      (util::Retry<Terminate, Fail>(
           10, time::seconds(0.01),
           [&invocation_count]() { ++invocation_count; throw Terminate(); })),
      Terminate);
  EXPECT_EQ(1, invocation_count);
}

TEST(RetryTest, RetryWhileFail) {
  int invocation_count = 0;
  EXPECT_THROW(
      (util::Retry<Terminate, Fail>(
           10, time::seconds(0.01),
           [&invocation_count]() {
             if (++invocation_count < 3) {
               throw Fail();
             } else {
               throw Terminate();
             }
           })),
      Terminate);
  EXPECT_EQ(3, invocation_count);
}

TEST(RetryTest, RetryWithSubclass) {
  int invocation_count = 0;
  EXPECT_THROW((util::Retry<TerminateSubclass, Fail>(
      10, time::seconds(0.01),
      [&invocation_count]() {
        if (++invocation_count < 5) {
          throw Fail();
        } else {
          throw TerminateSubclass();
        }
      })), TerminateSubclass);
  EXPECT_EQ(5, invocation_count);
}
}  // namespace
}  // namespace google
