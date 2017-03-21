#ifndef FORMAT_H_
#define FORMAT_H_

#include <map>
#include <string>

namespace format {

// A representation of format substitution errors.
class Exception {
 public:
  Exception(const std::string& what) : explanation_(what) {}
  const std::string& what() const { return explanation_; }
 private:
  std::string explanation_;
};

// Format string substitution.
// Placeholder format is '{{name}}'.
std::string Substitute(const std::string& format,
                       const std::map<std::string, std::string>&& params)
    throw(Exception);

}  // format

#endif  // FORMAT_H_
