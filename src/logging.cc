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

#include "logging.h"

#include <chrono>
#include <iomanip>
#include <iostream>
#include <thread>

#include "time.h"

namespace google {

Logger::Logger(const char* file, int line, Severity severity, LogStream* stream)
    : file_(file), line_(line), severity_(severity), stream_(stream)
{
  const std::time_t now_c =
      std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
  std::tm local_time = safe_localtime(&now_c);
  // GCC 4.x does not implement std::put_time. Sigh.
  char time_val[20];
  std::strftime(time_val, sizeof(time_val), "%m%d %H:%M:%S ", &local_time);
  (*this) << kSeverities_[severity]
          << time_val
          << std::hex << std::this_thread::get_id() << std::dec << " "
          << file_ << ":" << line_ << " ";
}

Logger::~Logger() {
  flush();
}

Logger& Logger::flush() {
  stream_->write(str());
  return *this;
}

LogStream default_log_stream(std::cerr);

//constexpr char Logger::kSeverities_[] = "DIWE";

constexpr char Logger::kSeverities_[];

}
