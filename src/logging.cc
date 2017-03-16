#include "logging.h"

#include <chrono>
#include <iomanip>
#include <iostream>
#include <thread>

namespace google {

namespace {

std::mutex localtime_mutex;
std::mutex gmtime_mutex;

}

std::tm safe_localtime(const std::time_t* t) {
  std::unique_lock<std::mutex> l(localtime_mutex);
  return *std::localtime(t);
}

std::tm safe_gmtime(const std::time_t* t) {
  std::unique_lock<std::mutex> l(gmtime_mutex);
  return *std::gmtime(t);
}

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

//constexpr char Logger::kSeverities_[] = "DIWE";

LogStream default_log_stream(std::cerr);

constexpr char Logger::kSeverities_[];

}
