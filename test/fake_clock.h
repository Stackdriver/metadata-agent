#ifndef FAKE_CLOCK_H_
#define FAKE_CLOCK_H_

#include <chrono>
#include <cstdint>
#include <ratio>

namespace google {
namespace testing {

class FakeClock {
 public:
  // Requirements for Clock, see:
  // http://en.cppreference.com/w/cpp/named_req/Clock.
  using rep = uint64_t;
  using period = std::ratio<1l, 1000000000l>;
  using duration = std::chrono::duration<rep, period>;
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

#endif  // FAKE_CLOCK_H_
