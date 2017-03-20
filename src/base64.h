#ifndef BASE64_H_
#define BASE64_H_

#include <string>

namespace base64 {

std::string Encode(const std::string &source);

std::string Decode(const std::string &source);

}

#endif  // BASE64_H_
