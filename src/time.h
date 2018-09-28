/*
 * Copyright 2017 Google Inc.
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
#ifndef TIME_H_
#define TIME_H_

#include <chrono>
#include <ctime>
#include <memory>
#include <string>

#include "logging.h"

namespace google {

using time_point = std::chrono::time_point<std::chrono::system_clock,
                                           std::chrono::nanoseconds>;

namespace time {

using seconds = std::chrono::duration<double, std::chrono::seconds::period>;

inline double SecondsSinceEpoch(const time_point& t) {
  return std::chrono::duration_cast<std::chrono::seconds>(
      t.time_since_epoch()).count();
}

namespace rfc3339 {

// Time conversions.
std::string ToString(const time_point& t);
time_point FromString(const std::string& s);

}

// Thread-safe versions of std:: functions.
std::tm safe_localtime(const std::time_t* t);
std::tm safe_gmtime(const std::time_t* t);

}

// Abstract class for a timer.
class Timer {
 public:
  // Initializes the timer.
  virtual void Init() = 0;

  // Waits for one duration to pass.  Returns false if the timer was
  // canceled while waiting.
  virtual bool Wait(time::seconds duration) = 0;

  // Cancels the timer.
  virtual void Cancel() = 0;
};

// Implementation of a timer parameterized over a clock type.
template<typename Clock>
class TimerImpl : public Timer {
 public:
  TimerImpl(bool verbose, const std::string& name)
      : verbose_(verbose), name_(name) {}
  void Init() override {
    timer_.lock();
    if (verbose_) {
      LOG(INFO) << "Locked timer for " << name_;
    }
  }
  bool Wait(time::seconds duration) override {
    // An unlocked timer means the wait is cancelled.
    auto start = Clock::now();
    auto wakeup = start + duration;
    if (verbose_) {
      LOG(INFO) << "Trying to unlock the timer for " << name_;
    }
    while (!timer_.try_lock_until(wakeup)) {
      auto now = Clock::now();
      // Detect spurious wakeups.
      if (now < wakeup) {
        continue;
      }
      if (verbose_) {
        LOG(INFO) << " Timer unlock timed out after "
                  << std::chrono::duration_cast<time::seconds>(now-start).count()
                  << "s (good) for " << name_;
      }
      return true;
    }
    return false;
  }
  void Cancel() override {
    timer_.unlock();
    if (verbose_) {
      LOG(INFO) << "Unlocked timer for " << name_;
    }
  }
 private:
  std::timed_mutex timer_;
  bool verbose_;
  std::string name_;
};

// Abstract class for tracking an expiration time.
class Expiration {
 public:
  virtual ~Expiration() = default;

  // Returns true if the expiration time has passed.
  virtual bool IsExpired() = 0;

  // Resets the expiration time to the given number of seconds from
  // now.
  virtual void Reset(std::chrono::seconds duration) = 0;
};

// Implementation of an expiration parameterized over a clock type.
template<typename Clock>
class ExpirationImpl : public Expiration {
 public:
  static std::unique_ptr<Expiration> New() {
    return std::unique_ptr<Expiration>(new ExpirationImpl<Clock>());
  }

  bool IsExpired() override {
    return token_expiration_ < Clock::now();
  }

  void Reset(std::chrono::seconds duration) override {
    token_expiration_ = Clock::now() + duration;
  }

 private:
  typename Clock::time_point token_expiration_;
};

}

#endif  // TIME_H_
