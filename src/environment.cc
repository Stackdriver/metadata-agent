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

#include "environment.h"

#include <boost/network/protocol/http/client.hpp>
#include <fstream>
#include <sstream>

#include "format.h"
#include "logging.h"

namespace http = boost::network::http;

namespace google {

namespace {

class NoCredentials {
 public:
  NoCredentials(const std::string& filename) : filename_(filename) {}
  const std::string& filename() const { return filename_; }
 private:
  std::string filename_;
};

json::value ReadCredentials(
    const std::string& credentials_file, bool verbose_logging)
    throw(json::Exception, NoCredentials) {
  std::string filename = credentials_file;
  if (filename.empty()) {
    const char* creds_env_var = std::getenv("GOOGLE_APPLICATION_CREDENTIALS");
    if (creds_env_var) {
      filename = creds_env_var;
    } else {
      // TODO: On Windows, "C:/ProgramData/Google/Auth/application_default_credentials.json"
      filename = "/etc/google/auth/application_default_credentials.json";
    }
  }
  std::ifstream input(filename);
  if (!input.good()) {
    if (verbose_logging) {
      LOG(INFO) << "Missing credentials file " << filename;
    }
    throw NoCredentials(filename);
  }
  if (verbose_logging) {
    LOG(INFO) << "Reading credentials from " << filename;
  }
  json::value creds_json = json::Parser::FromStream(input);
  if (creds_json == nullptr) {
    throw json::Exception("Could not parse credentials from " + filename);
  }
  if (verbose_logging) {
    LOG(INFO) << "Retrieved credentials from " << filename << ": " << *creds_json;
  }
  return std::move(creds_json);
}

constexpr const char kGceMetadataServerAddress[] =
    "http://metadata.google.internal./computeMetadata/v1/";

constexpr const char kGceInstanceResourceType[] = "gce_instance";

}

Environment::Environment(const Configuration& config)
    : config_(config), application_default_credentials_read_(false) {}

std::string Environment::GetMetadataString(const std::string& path) const {
  http::client::options options;
  http::client client(options.timeout(1));
  http::client::request request(kGceMetadataServerAddress + path);
  request << boost::network::header("Metadata-Flavor", "Google");
  try {
    http::client::response response = client.get(request);
    if (status(response) < 300) {
      return body(response);
    } else {
      throw boost::system::system_error(
          boost::system::errc::make_error_code(boost::system::errc::not_connected),
          format::Substitute("Server responded with '{{message}}' ({{code}})",
                             {{"message", status_message(response)},
                              {"code", format::str(status(response))}}));
    }
  } catch (const boost::system::system_error& e) {
    LOG(ERROR) << "Exception: " << e.what()
               << ": '" << kGceMetadataServerAddress << path << "'";
    return "";
  }
}

const std::string& Environment::NumericProjectId() const {
  std::lock_guard<std::mutex> lock(mutex_);
  if (project_id_.empty()) {
    if (!config_.ProjectId().empty()) {
      project_id_ = config_.ProjectId();
    } else {
      ReadApplicationDefaultCredentials();
      if (!client_email_.empty()) {
        // Extract from credentials.
        // New-style emails (string@project.iam.gserviceaccount.com).
        // Old-style emails (projectnumber-hash@developer.gserviceaccount.com).
        std::string::size_type new_style =
            client_email_.find(".iam.gserviceaccount.com");
        std::string::size_type old_style =
            client_email_.find("@developer.gserviceaccount.com");
        if (new_style != std::string::npos) {
          std::string::size_type at = client_email_.find('@');
          if (at != std::string::npos) {
            project_id_ = client_email_.substr(at + 1, new_style - at - 1);
            LOG(INFO) << "Found project id in credentials: " << project_id_;
          }
        } else if (old_style != std::string::npos) {
          std::string::size_type dash = client_email_.find('-');
          if (dash != std::string::npos) {
            project_id_ = client_email_.substr(0, dash);
            LOG(INFO) << "Found project id in credentials: " << project_id_;
          }
        } else {
          LOG(ERROR) << "Unable to extract project id from " << client_email_;
        }
      }
      if (project_id_.empty()) {
        // Query the metadata server.
        // TODO: Other sources.
        if (config_.VerboseLogging()) {
          LOG(INFO) << "Getting project id from metadata server";
        }
        project_id_ = GetMetadataString("project/numeric-project-id");
        LOG(INFO) << "Got project id from metadata server: " << project_id_;
      }
    }
  }
  return project_id_;
}

const std::string& Environment::InstanceZone() const {
  std::lock_guard<std::mutex> lock(mutex_);
  if (zone_.empty()) {
    if (!config_.InstanceZone().empty()) {
      zone_ = config_.InstanceZone();
    } else {
      // Query the metadata server.
      // TODO: Other sources?
      zone_ = GetMetadataString("instance/zone");
      zone_ = zone_.substr(zone_.rfind('/') + 1);
    }
  }
  return zone_;
}

const std::string& Environment::InstanceId() const {
  std::lock_guard<std::mutex> lock(mutex_);
  if (instance_id_.empty()) {
    if (!config_.InstanceId().empty()) {
      instance_id_ = config_.InstanceId();
    } else {
      // Query the metadata server.
      // TODO: Other sources?
      instance_id_ = GetMetadataString("instance/id");
    }
  }
  return instance_id_;
}

const std::string& Environment::InstanceResourceType() const {
  std::lock_guard<std::mutex> lock(mutex_);
  if (instance_resource_type_.empty()) {
    if (!config_.InstanceResourceType().empty()) {
      instance_resource_type_ = config_.InstanceResourceType();
    } else {
      // Default to a GCE instance.
      // TODO: Detect other instance resources.
      instance_resource_type_ = kGceInstanceResourceType;
    }
  }
  return instance_resource_type_;
}

const std::string& Environment::KubernetesClusterName() const {
  std::lock_guard<std::mutex> lock(mutex_);
  if (kubernetes_cluster_name_.empty()) {
    if (!config_.KubernetesClusterName().empty()) {
      kubernetes_cluster_name_ = config_.KubernetesClusterName();
    } else {
      // Query the metadata server.
      // TODO: Other sources? kube-env?
      kubernetes_cluster_name_ =
          GetMetadataString("instance/attributes/cluster-name");
    }
  }
  return kubernetes_cluster_name_;
}

const std::string& Environment::KubernetesClusterLocation() const {
  std::lock_guard<std::mutex> lock(mutex_);
  if (kubernetes_cluster_location_.empty()) {
    if (!config_.KubernetesClusterLocation().empty()) {
      kubernetes_cluster_location_ = config_.KubernetesClusterLocation();
    } else {
      // Query the metadata server.
      kubernetes_cluster_location_ =
          GetMetadataString("instance/attributes/cluster-location");
      // Fall back on kube-env for older clusters.
      if (kubernetes_cluster_location_.empty()) {
        // Get the kube-env.
        const std::string kube_env =
            GetMetadataString("instance/attributes/kube-env");
        // kube-env is a list of NAME: VALUE pairs, one per line, unsorted.
        // The actual location is in the ZONE variable.
        // TODO: Refactor this into GetKubeEnv("ZONE") in kubernetes.cc.
        std::istringstream in(kube_env);
        for (std::string line; std::getline(in, line); ) {
          if (line.find("ZONE: ") == 0) {
            kubernetes_cluster_location_ = line.substr(line.find(':') + 2);
            break;
          }
        }
      }
    }
  }
  return kubernetes_cluster_location_;
}

const std::string& Environment::CredentialsClientEmail() const {
  std::lock_guard<std::mutex> lock(mutex_);
  ReadApplicationDefaultCredentials();
  return client_email_;
}

const std::string& Environment::CredentialsPrivateKey() const {
  std::lock_guard<std::mutex> lock(mutex_);
  ReadApplicationDefaultCredentials();
  return private_key_;
}

void Environment::ReadApplicationDefaultCredentials() const {
  if (application_default_credentials_read_) {
    return;
  }
  try {
    json::value creds_json = ReadCredentials(config_.CredentialsFile(),
                                             config_.VerboseLogging());

    const json::Object* creds = creds_json->As<json::Object>();

    client_email_ = creds->Get<json::String>("client_email");
    private_key_ = creds->Get<json::String>("private_key");

    LOG(INFO) << "Retrieved private key from application default credentials";
  } catch (const json::Exception& e) {
    LOG(ERROR) << e.what();
  } catch (const NoCredentials& e) {
    LOG(INFO) << "No credentials found at " << e.filename();
  }
  application_default_credentials_read_ = true;
}

}  // google
