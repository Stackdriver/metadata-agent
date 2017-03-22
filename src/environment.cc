#include "environment.h"

#include <boost/network/protocol/http/client.hpp>

#include "logging.h"

namespace http = boost::network::http;

namespace google {

Environment::Environment(const MetadataAgentConfiguration& config)
    : config_(config) {}

std::string Environment::GetMetadataString(const std::string& path) const {
  http::client client;
  http::client::request request(
      "http://metadata.google.internal/computeMetadata/v1/" + path);
  request << boost::network::header("Metadata-Flavor", "Google");
  try {
    http::client::response response = client.get(request);
    return body(response);
  } catch (const boost::system::system_error& e) {
    LOG(ERROR) << "Exception: " << e.what()
               << ": 'http://metadata.google.internal/computeMetadata/v1/"
               << path << "'";
    return "";
  }
}

std::string Environment::NumericProjectId() const {
  static std::string project_id;
  if (project_id.empty()) {
    if (!config_.ProjectId().empty()) {
      project_id = config_.ProjectId();
    } else {
      // Query the metadata server.
      // TODO: Other sources.
      project_id = GetMetadataString("project/numeric-project-id");
    }
  }
  return project_id;
}

std::string Environment::InstanceZone() const {
  static std::string zone;
  if (zone.empty()) {
    if (!config_.InstanceZone().empty()) {
      zone = config_.InstanceZone();
    } else {
      // Query the metadata server.
      // TODO: Other sources?
      zone = GetMetadataString("instance/zone");
    }
    if (zone.empty()) {
      zone = "1234567890";
    }
  }
  return zone;
}

}  // google
