#ifndef ENVIRONMENT_H_
#define ENVIRONMENT_H_

#include <string>

#include "configuration.h"

namespace google {

class Environment {
 public:
  Environment(const MetadataAgentConfiguration& config);
  std::string NumericProjectId() const;
  std::string InstanceZone() const;

 private:
  friend class OAuth2;

  const MetadataAgentConfiguration& config_;
};

}  // google

#endif  // ENVIRONMENT_H_
