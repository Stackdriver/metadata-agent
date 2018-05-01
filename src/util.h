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
#ifndef UTIL_H_
#define UTIL_H_

#include <functional>
#include <thread>

#include "time.h"

namespace google {

namespace util {

// Executes the work function while it throws exception Fail, up to a given
// number of times, with a delay between executions. If it throws exception
// Terminate, which may be a subclass of Fail, pass it through (no retries).
// Any other exception outside of the Fail hierarchy will be passed through.
template<class Terminate, class Fail>
void Retry(int max_retries, time::seconds delay,
           std::function<void()> work);

template<class Terminate, class Fail>
void Retry(int max_retries, time::seconds delay,
           std::function<void()> work) {
  for (int i = 0; true; ++i) {
    try {
      work();
      return;
    } catch (const Terminate& e) {  // Catch first due to inheritance.
      throw e;
    } catch (const Fail& e) {
      if (i < max_retries - 1) {
        std::this_thread::sleep_for(delay);
      } else {
        throw e;
      }
    }
  }
}

}  // util

}  // google

#endif  // UTIL_H_
