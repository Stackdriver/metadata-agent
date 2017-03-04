#include "logging.h"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <thread>

namespace google {

Logger::Logger(const char* file, int line, Severity severity, LogStream* stream)
    : file_(file), line_(line), severity_(severity), stream_(stream)
{
  const std::time_t now_c =
      std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
  // GCC 4.x does not implement std::put_time. Sigh.
  char time_val[20];
  strftime(time_val, sizeof(time_val),
           "%m%d %H:%M:%S ",
           std::localtime(&now_c));
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

//constexpr char Logger::kSeverities_[] = "DIWE";

LogStream default_log_stream(std::cerr);

constexpr char Logger::kSeverities_[];

}
