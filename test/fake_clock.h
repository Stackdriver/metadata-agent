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
#ifndef FAKE_CLOCK_H_
#define FAKE_CLOCK_H_

#include <chrono>
#include <cstdint>
#include <mutex>
#include <ratio>

#include "../src/time.h"

namespace google {
namespace testing {

class FakeClock {
 public:
  // Requirements for Clock, see:
  // http://en.cppreference.com/w/cpp/named_req/Clock.
  using rep = time::seconds::rep;
  using period = time::seconds::period;
  using duration = time::seconds;
  using time_point = std::chrono::time_point<FakeClock>;
  static constexpr const bool is_steady = false;

  static time_point now() {
    std::lock_guard<std::mutex> guard(mutex_);
    time_point now = now_;
    now_ = future_now_;
    return now;
  }

  // Increment fake clock's internal time.
  static void Advance(duration d) {
    std::lock_guard<std::mutex> guard(mutex_);
    now_ += d;
    future_now_ = now_;
  }

  // Increment fake clock's internal time after the next call to now().
  static void AdvanceAfterNextNowCall(duration d) {
    std::lock_guard<std::mutex> guard(mutex_);
    future_now_ = now_ + d;
  }

 private:
  FakeClock() = delete;
  ~FakeClock() = delete;
  FakeClock(FakeClock const&) = delete;

  static time_point now_;
  static time_point future_now_;
  static std::mutex mutex_;
};

}  // namespace testing
}  // namespace google

namespace boost {
namespace asio {
// Specialize boost::asio::wait_traits so that
// basic_waitable_timer::async_wait() will work with a FakeClock.
template<>
struct wait_traits<google::testing::FakeClock> {
  static google::testing::FakeClock::duration to_wait_duration(
      const google::testing::FakeClock::duration& d) {
    // This is the idiom for using a timer with a fake clock; for
    // example, see the "Jumping Through Time" section of
    // http://blog.think-async.com/2007/08/time-travel.html.
    //
    // Note that basic_waitable_timer uses WaitTraits, which require
    // to_wait_duration, not to_posix_duration:
    // https://www.boost.org/doc/libs/1_66_0/doc/html/boost_asio/reference/WaitTraits.html
    return std::min(d, google::time::seconds(0.001));
  }
};
}  // namespace asio
}  // namespace boost

namespace std {
// Allow using std::timed_mutex::try_lock_until with a FakeClock.
template<>
inline bool timed_mutex::try_lock_until<
    google::testing::FakeClock, google::testing::FakeClock::duration>(
    const google::testing::FakeClock::time_point& timeout_time) {
  do {
    if (try_lock()) {
      return true;
    }
  } while (google::testing::FakeClock::now() < timeout_time);
  return false;
}
}  // namespace std

#endif  // FAKE_CLOCK_H_
