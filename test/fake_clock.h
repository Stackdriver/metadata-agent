#ifndef FAKE_CLOCK_H_
#define FAKE_CLOCK_H_

#include <chrono>
#include <cstdint>
#include <ratio>

namespace google {
namespace testing {

class FakeClock {
 public:
  // Requirements for Clock, see http://en.cppreference.com/w/cpp/named_req/Clock
  typedef uint64_t rep;
  typedef std::ratio<1l, 1000000000l> period;
  typedef std::chrono::duration<rep, period> duration;
  typedef std::chrono::time_point<FakeClock> time_point;
  static const bool is_steady;
  static time_point now() noexcept;

  static void Advance(duration d) noexcept;

 private:
  FakeClock() = delete;
  ~FakeClock() = delete;
  FakeClock(FakeClock const&) = delete;

  static time_point now_;
};

}  // testing
}  // google

#endif  // FAKE_CLOCK_H_
