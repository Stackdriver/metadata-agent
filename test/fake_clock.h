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
  static time_point now() { return now_; }

  // Increment fake clock's internal time.
  static void Advance(duration d) { now_ += d; }

 private:
  FakeClock() = delete;
  ~FakeClock() = delete;
  FakeClock(FakeClock const&) = delete;

  static time_point now_;
};

}  // namespace testing
}  // namespace google

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
