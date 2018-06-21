#include "fake_clock.h"

namespace google {
namespace testing {

FakeClock::time_point FakeClock::now_;

const bool FakeClock::is_steady = false;

FakeClock::time_point FakeClock::now() noexcept {
  return now_;
}

void FakeClock::Advance(duration d) noexcept {
  now_ += d;
}

}  // testing
}  // google
