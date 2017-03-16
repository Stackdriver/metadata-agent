#ifndef TIME_H_
#define TIME_H_

#include <chrono>
#include <ctime>
#include <string>

namespace google {

namespace rfc3339 {

// Time conversions.
std::string ToString(const std::chrono::system_clock::time_point& t);
std::chrono::system_clock::time_point FromString(const std::string& s);

}

// Thread-safe versions of std:: functions.
std::tm safe_localtime(const std::time_t* t);
std::tm safe_gmtime(const std::time_t* t);

}

#endif  // TIME_H_
