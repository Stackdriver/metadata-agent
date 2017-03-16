#ifndef LOGGING_H_
#define LOGGING_H_

#include <ctime>
#include <mutex>
#include <sstream>

namespace google {

class LogStream {
 public:
  LogStream(std::ostream& out)
      : out_(&out) {}
  ~LogStream() {}
  void set_stream(std::ostream& out) {
    out_->flush();
    out_ = &out;
  }
  void write(const std::string& line) {
    std::lock_guard<std::mutex> lock(mutex_);
    (*out_) << line << std::endl;
  }

 private:
  std::mutex mutex_;
  std::ostream* out_;
};

class Logger : public std::ostringstream {
 public:
  enum Severity { DEBUG, INFO, WARNING, ERROR };
  Logger(const char* file, int line, Severity severity, LogStream* stream);
  // For better experience, only call flush() once per logger.
  Logger& flush();
  ~Logger();

 private:
  const char* file_;
  int line_;
  Severity severity_;
  LogStream* stream_;

  static constexpr char kSeverities_[] = "DIWE";
};

// Global default log stream;
extern LogStream default_log_stream;

}

#define LOG(SEVERITY) ::google::Logger(__FILE__, __LINE__, \
                                       ::google::Logger::SEVERITY, \
                                       &::google::default_log_stream)

#endif  // LOGGING_H_
