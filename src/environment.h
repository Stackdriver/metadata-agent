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
#ifndef ENVIRONMENT_H_
#define ENVIRONMENT_H_

#include <mutex>
#include <string>

#include "configuration.h"
#include "json.h"

namespace google {

class Environment {
 public:
  Environment(const Configuration& config);

  const std::string& NumericProjectId() const;
  const std::string& InstanceResourceType() const;
  const std::string& InstanceId() const;
  const std::string& InstanceZone() const;
  const std::string& KubernetesClusterName() const;
  const std::string& KubernetesClusterLocation() const;
  const std::string& CredentialsClientEmail() const;
  const std::string& CredentialsPrivateKey() const;

  std::string GetMetadataString(const std::string& path) const;

  const Configuration& config() const {
    return config_;
  }

 private:
  void ReadApplicationDefaultCredentials() const;

  const Configuration& config_;

  // Cached data.
  mutable std::mutex mutex_;
  mutable std::string project_id_;
  mutable std::string zone_;
  mutable std::string instance_id_;
  mutable std::string instance_resource_type_;
  mutable std::string kubernetes_cluster_name_;
  mutable std::string kubernetes_cluster_location_;
  mutable std::string client_email_;
  mutable std::string private_key_;
  mutable bool application_default_credentials_read_;
};

}  // google

#endif  // ENVIRONMENT_H_
