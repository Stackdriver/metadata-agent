#ifndef ENVIRONMENT_H_
#define ENVIRONMENT_H_

#include <mutex>
#include <string>

#include "configuration.h"
#include "json.h"

namespace google {

class Environment {
 public:
  Environment(const MetadataAgentConfiguration& config);

  const std::string& NumericProjectId() const;
  const std::string& InstanceZone() const;
  const std::string& CredentialsClientEmail() const;
  const std::string& CredentialsPrivateKey() const;

  std::string GetMetadataString(const std::string& path) const;

 private:
  void ReadApplicationDefaultCredentials() const;

  const MetadataAgentConfiguration& config_;

  // Cached data.
  mutable std::mutex mutex_;
  mutable std::string project_id_;
  mutable std::string zone_;
  mutable std::string client_email_;
  mutable std::string private_key_;
  mutable bool application_default_credentials_read_;
};

}  // google

#endif  // ENVIRONMENT_H_
