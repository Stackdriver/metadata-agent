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

#include "kubernetes.h"

#include <algorithm>
#include <boost/algorithm/string/join.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/asio/ip/host_name.hpp>
#include <boost/network/protocol/http/client.hpp>
#include <boost/range/iterator_range.hpp>
#include <chrono>
#include <cstddef>
#include <fstream>
#include <sstream>
#include <tuple>

#include "configuration.h"
#include "format.h"
#include "http_common.h"
#include "instance.h"
#include "json.h"
#include "logging.h"
#include "resource.h"
#include "store.h"
#include "time.h"
#include "util.h"

namespace http = boost::network::http;

namespace google {

namespace {

constexpr const int kKubernetesValidationRetryLimit = 10;
constexpr const int kKubernetesValidationRetryDelaySeconds = 1;

#if 0
constexpr const char kKubernetesEndpointHost[] = "https://kubernetes.default.svc";
#endif
constexpr const char kKubernetesEndpointPath[] = "/api/v1";

constexpr const char kGkeContainerResourcePrefix[] = "gke_container";
constexpr const char kK8sContainerResourcePrefix[] = "k8s_container";
constexpr const char kK8sPodResourcePrefix[] = "k8s_pod";
constexpr const char kK8sNodeResourcePrefix[] = "k8s_node";

constexpr const char kNodeSelectorPrefix[] = "?fieldSelector=spec.nodeName%3D";

constexpr const char kDockerIdPrefix[] = "docker://";

constexpr const char kServiceAccountDirectory[] =
    "/var/run/secrets/kubernetes.io/serviceaccount";
constexpr const char kKubernetesSchemaNameFormat[] =
    "//container.googleapis.com/resourceTypes/{{type}}/versions/{{version}}";

constexpr const char kClusterFullNameFormat[] =
    "//container.googleapis.com/projects/{{project_id}}/{{location_type}}/"
    "{{location}}/clusters/{{cluster_name}}";
constexpr const char kK8sFullNameFormat[] =
    "{{cluster_full_name}}/k8s/{{relative_link}}";

constexpr const char kK8sCoreGroupResourceTypeFormat[] = "io.k8s.{{kind}}";
constexpr const char kK8sNamedGroupsResourceTypeFormat[] =
    "io.k8s.{{group_name}}.{{kind}}";

// TODO: There seems to be a Kubernetes API bug with watch=true, so we
// explicitly add "watch" to the path.
constexpr const char kK8sWatchPathFormat[] =
    "/{{api_prefix}}/{{api_version}}/watch/{{plural_kind}}{{selector}}";

// Returns the full path to the secret filename.
std::string SecretPath(const std::string& secret) {
  return std::string(kServiceAccountDirectory) + "/" + secret;
}

// Reads a Kubernetes service account secret file into the provided string.
// Returns true if the file was read successfully.
bool ReadServiceAccountSecret(
    const std::string& secret, std::string& destination, bool verbose) {
  std::string filename(SecretPath(secret));
  std::ifstream input(filename);
  if (!input.good()) {
    if (verbose) {
      LOG(ERROR) << "Missing " << filename;
    }
    return false;
  }
  if (verbose) {
    LOG(INFO) << "Reading from " << filename;
  }
  std::getline(input, destination);
  return !input.fail();
}

// Builds the resource type and version of a Kubernetes resource given its API
// version and kind.
const std::pair<std::string, std::string> TypeAndVersion(
    const std::string& api_version, const std::string& kind) {
  std::vector<std::string> slash_split;
  boost::algorithm::split(
      slash_split, api_version, boost::algorithm::is_any_of("/"));
  if (slash_split.size() > 2) {
    LOG(ERROR) << "api_version must have at most one '/' in it: " << api_version;
    assert(false);
  }
  std::string resource_type;
  std::string version;
  if (slash_split.size() == 1) {
    resource_type = format::Substitute(kK8sCoreGroupResourceTypeFormat,
                                       {{"kind", kind}});
    version = api_version;
  } else {
    resource_type = format::Substitute(kK8sNamedGroupsResourceTypeFormat,
                                       {{"group_name", slash_split[0]},
                                        {"kind", kind}});
    version = slash_split[1];
  }
  return std::make_pair(resource_type, version);
}

}

// A subclass of QueryException to represent non-retriable errors.
class KubernetesReader::NonRetriableError
    : public KubernetesReader::QueryException {
 public:
  NonRetriableError(const std::string& what) : QueryException(what) {}
};

KubernetesReader::KubernetesReader(const Configuration& config,
                                   HealthChecker* health_checker)
    : config_(config), environment_(config), health_checker_(health_checker),
      service_account_directory_(kServiceAccountDirectory) {}

std::string KubernetesReader::SecretPath(const std::string& secret) const {
  return service_account_directory_ + "/" + secret;
}

bool KubernetesReader::ReadServiceAccountSecret(
    const std::string& secret, std::string& destination, bool verbose) const {
  std::string filename(SecretPath(secret));
  std::ifstream input(filename);
  if (!input.good()) {
    if (verbose) {
      LOG(ERROR) << "Missing " << filename;
    }
    return false;
  }
  if (verbose) {
    LOG(INFO) << "Reading from " << filename;
  }
  std::getline(input, destination);
  return !input.fail();
}

MetadataUpdater::ResourceMetadata KubernetesReader::GetObjectMetadata(
    const json::Object* object, Timestamp collected_at, bool is_deleted,
    KubernetesReader::ResourceMappingCallback resource_mapping_cb) const
    throw(json::Exception) {
  const std::string cluster_location = environment_.KubernetesClusterLocation();

  const std::string kind = object->Get<json::String>("kind");
  const std::string api_version = object->Get<json::String>("apiVersion");
  const json::Object* metadata = object->Get<json::Object>("metadata");
  const std::string self_link = metadata->Get<json::String>("selfLink");
  const std::pair<std::string, std::string> type_and_version =
      TypeAndVersion(api_version, kind);
  const std::string type = type_and_version.first;
  const std::string version = type_and_version.second;

  const std::string schema =
      format::Substitute(kKubernetesSchemaNameFormat,
                         {{"type", type}, {"version", version}});
  const std::string created_str =
      metadata->Get<json::String>("creationTimestamp");
  Timestamp created_at = time::rfc3339::FromString(created_str);
  const std::string resource_full_name = FullResourceName(self_link);

  if (config_.VerboseLogging()) {
    LOG(INFO) << "Raw resource metadata for full name: " << resource_full_name
              << ": " << *object;
  }
  IdsAndMR ids_and_mr = resource_mapping_cb(object);
  return MetadataUpdater::ResourceMetadata(
      ids_and_mr.first, ids_and_mr.second,
      MetadataStore::Metadata(
          resource_full_name, type, cluster_location, version, schema,
          is_deleted, collected_at, object->Clone())
  );
}

KubernetesReader::IdsAndMR KubernetesReader::NodeResourceMappingCallback(
    const json::Object* node) const throw(json::Exception) {
  const std::string cluster_name = environment_.KubernetesClusterName();
  const std::string cluster_location = environment_.KubernetesClusterLocation();

  const json::Object* metadata = node->Get<json::Object>("metadata");
  const std::string node_name = metadata->Get<json::String>("name");
  const MonitoredResource k8s_node("k8s_node", {
    {"cluster_name", cluster_name},
    {"node_name", node_name},
    {"location", cluster_location},
  });
  const std::string k8s_node_name = boost::algorithm::join(
      std::vector<std::string>{kK8sNodeResourcePrefix, node_name},
      config_.MetadataApiResourceTypeSeparator());

  return {std::vector<std::string>{k8s_node_name}, k8s_node};
}

KubernetesReader::IdsAndMR KubernetesReader::PodResourceMappingCallback(
    const json::Object* pod) const throw(json::Exception) {
  const std::string cluster_name = environment_.KubernetesClusterName();
  const std::string cluster_location = environment_.KubernetesClusterLocation();

  const json::Object* metadata = pod->Get<json::Object>("metadata");
  const std::string namespace_name = metadata->Get<json::String>("namespace");
  const std::string pod_name = metadata->Get<json::String>("name");
  const std::string pod_id = metadata->Get<json::String>("uid");

  const MonitoredResource k8s_pod("k8s_pod", {
    {"cluster_name", cluster_name},
    {"namespace_name", namespace_name},
    {"pod_name", pod_name},
    {"location", cluster_location},
  });
  const std::string k8s_pod_id = boost::algorithm::join(
      std::vector<std::string>{kK8sPodResourcePrefix, pod_id},
      config_.MetadataApiResourceTypeSeparator());
  const std::string k8s_pod_name = boost::algorithm::join(
      std::vector<std::string>{kK8sPodResourcePrefix, namespace_name, pod_name},
      config_.MetadataApiResourceTypeSeparator());

  return {std::vector<std::string>{k8s_pod_id, k8s_pod_name}, k8s_pod};
}

KubernetesReader::IdsAndMR KubernetesReader::EmptyResourceMappingCallback(
    const json::Object* object) const {
  return {std::vector<std::string>{}, MonitoredResource("", {})};
}

MetadataUpdater::ResourceMetadata KubernetesReader::GetContainerMetadata(
    const json::Object* pod, const json::Object* container_spec,
    const json::Object* container_status, Timestamp collected_at, bool is_deleted) const throw(json::Exception) {
  const std::string cluster_name = environment_.KubernetesClusterName();
  const std::string location = environment_.KubernetesClusterLocation();

  const json::Object* metadata = pod->Get<json::Object>("metadata");
  const std::string namespace_name = metadata->Get<json::String>("namespace");
  const std::string pod_name = metadata->Get<json::String>("name");
  const std::string pod_id = metadata->Get<json::String>("uid");
  const std::string container_name = container_spec->Get<json::String>("name");

  const MonitoredResource k8s_container("k8s_container", {
    {"cluster_name", cluster_name},
    {"namespace_name", namespace_name},
    {"pod_name", pod_name},
    {"container_name", container_name},
    {"location", location},
  });

  const std::string k8s_container_pod = boost::algorithm::join(
      std::vector<std::string>{kK8sContainerResourcePrefix, pod_id, container_name},
      config_.MetadataApiResourceTypeSeparator());
  const std::string k8s_container_name = boost::algorithm::join(
      std::vector<std::string>{kK8sContainerResourcePrefix, namespace_name, pod_name, container_name},
      config_.MetadataApiResourceTypeSeparator());

  std::vector<std::string> local_resource_ids = {
    k8s_container_pod,
    k8s_container_name,
  };

  if (container_status && container_status->Has("containerID")) {
    std::size_t docker_prefix_length = sizeof(kDockerIdPrefix) - 1;

    const std::string docker_id =
        container_status->Get<json::String>("containerID");
    if (docker_id.compare(0, docker_prefix_length, kDockerIdPrefix) != 0) {
      LOG(ERROR) << "containerID "
                 << docker_id
                 << " does not start with " << kDockerIdPrefix
                 << " (" << docker_prefix_length << " chars)";
      docker_prefix_length = 0;
    }
    const std::string container_id = docker_id.substr(docker_prefix_length);
    const std::string k8s_container_id = boost::algorithm::join(
        std::vector<std::string>{kK8sContainerResourcePrefix, container_id},
        config_.MetadataApiResourceTypeSeparator());

    local_resource_ids.push_back(k8s_container_id);
  }

  return MetadataUpdater::ResourceMetadata(
      std::move(local_resource_ids),
      k8s_container,
      MetadataStore::Metadata::IGNORED()
  );
}

MetadataUpdater::ResourceMetadata KubernetesReader::GetLegacyResource(
    const json::Object* pod, const std::string& container_name) const
    throw(json::Exception) {
  const std::string instance_id = environment_.InstanceId();
  const std::string zone = environment_.InstanceZone();
  const std::string cluster_name = environment_.KubernetesClusterName();

  const json::Object* metadata = pod->Get<json::Object>("metadata");
  const std::string namespace_name = metadata->Get<json::String>("namespace");
  const std::string pod_name = metadata->Get<json::String>("name");
  const std::string pod_id = metadata->Get<json::String>("uid");

  const MonitoredResource gke_container("gke_container", {
    {"cluster_name", cluster_name},
    {"namespace_id", namespace_name},
    {"instance_id", instance_id},
    {"pod_id", pod_id},
    {"container_name", container_name},
    {"zone", zone},
  });

  const std::string gke_container_pod_id = boost::algorithm::join(
      std::vector<std::string>{kGkeContainerResourcePrefix, namespace_name, pod_id, container_name},
      config_.MetadataApiResourceTypeSeparator());
  const std::string gke_container_name = boost::algorithm::join(
      std::vector<std::string>{kGkeContainerResourcePrefix, namespace_name, pod_name, container_name},
      config_.MetadataApiResourceTypeSeparator());
  return MetadataUpdater::ResourceMetadata(
      std::vector<std::string>{gke_container_pod_id, gke_container_name},
      gke_container,
      MetadataStore::Metadata::IGNORED());
}

std::vector<MetadataUpdater::ResourceMetadata>
KubernetesReader::GetPodAndContainerMetadata(
    const json::Object* pod, Timestamp collected_at, bool is_deleted) const
    throw(json::Exception) {
  std::vector<MetadataUpdater::ResourceMetadata> result;

  const json::Object* metadata = pod->Get<json::Object>("metadata");
  const std::string pod_name = metadata->Get<json::String>("name");
  const std::string pod_id = metadata->Get<json::String>("uid");

  const json::Object* spec = pod->Get<json::Object>("spec");

  const json::Object* status = pod->Get<json::Object>("status");

  const json::Array* container_specs = spec->Get<json::Array>("containers");

  // Move the container statuses into a map from name to status.
  std::map<std::string, const json::Object*> container_status_by_name;
  if (status->Has("containerStatuses")) {
    const json::Array* container_statuses =
        status->Get<json::Array>("containerStatuses");

    for (const json::value& c_status : *container_statuses) {
      const json::Object* container_status = c_status->As<json::Object>();
      const std::string name = container_status->Get<json::String>("name");
      container_status_by_name.emplace(name, container_status);
    }
  }

  for (const json::value& c_spec : *container_specs) {
    if (config_.VerboseLogging()) {
      LOG(INFO) << "Container: " << *c_spec;
    }
    const json::Object* container_spec = c_spec->As<json::Object>();
    const std::string name = container_spec->Get<json::String>("name");
    auto status_it = container_status_by_name.find(name);
    const json::Object* container_status;
    if (status_it == container_status_by_name.end()) {
      container_status = nullptr;
      if (config_.VerboseLogging()) {
        LOG(INFO) << "No status for container " << name;
      }
    } else {
      container_status = status_it->second;
      if (config_.VerboseLogging()) {
        LOG(INFO) << "Container status: " << *container_status;
      }
    }
    result.emplace_back(GetLegacyResource(pod, name));
    result.emplace_back(
        GetContainerMetadata(pod, container_spec, container_status,
                             collected_at, is_deleted));
  }

  auto cb = [=](const json::Object* pod) {
    return PodResourceMappingCallback(pod);
  };
  result.emplace_back(GetObjectMetadata(pod, collected_at, is_deleted, cb));
  return std::move(result);
}

std::vector<MetadataUpdater::ResourceMetadata>
    KubernetesReader::MetadataQuery() const {
  if (config_.VerboseLogging()) {
    LOG(INFO) << "Kubernetes Query called";
  }
  std::vector<MetadataUpdater::ResourceMetadata> result;

  const std::string current_node = CurrentNode();

  if (config_.VerboseLogging()) {
    LOG(INFO) << "Current node is " << current_node;
  }

  const std::string node_name(
      config_.KubernetesClusterLevelMetadata() ? "" : current_node);

  try {
    json::value raw_nodes = QueryMaster(
        std::string(kKubernetesEndpointPath) + "/nodes/" + node_name);
    Timestamp collected_at = std::chrono::system_clock::now();

    if (!node_name.empty()) {
      // It's a single node object -- fake a NodeList.
      raw_nodes.reset(json::object({
        {"items", json::array({std::move(raw_nodes)})},
      }).release());
    }

    const json::Object* nodelist = raw_nodes->As<json::Object>();
    const json::Array* nodes_array = nodelist->Get<json::Array>("items");
    auto cb = [=](const json::Object* node) {
      return NodeResourceMappingCallback(node);
    };
    for (const json::value& raw_node : *nodes_array) {
      const json::Object* node = raw_node->As<json::Object>();
      result.emplace_back(
          GetObjectMetadata(node, collected_at, /*is_deleted=*/false, cb));
    }
  } catch (const json::Exception& e) {
    LOG(ERROR) << e.what();
  } catch (const QueryException& e) {
    // Already logged.
  }

  const std::string node_selector(kNodeSelectorPrefix + node_name);
  const std::string pod_label_selector(
      config_.KubernetesPodLabelSelector().empty()
      ? "" : "&" + config_.KubernetesPodLabelSelector());

  try {
    json::value podlist_response = QueryMaster(
        std::string(kKubernetesEndpointPath) + "/pods" + node_selector +
        pod_label_selector);
    Timestamp collected_at = std::chrono::system_clock::now();
    if (config_.VerboseLogging()) {
      LOG(INFO) << "Parsed pod list: " << *podlist_response;
    }
    const json::Object* podlist_object = podlist_response->As<json::Object>();
    const std::string api_version =
        podlist_object->Get<json::String>("apiVersion");
    const json::Array* pod_list = podlist_object->Get<json::Array>("items");
    for (const json::value& raw_pod : *pod_list) {
      try {
        if (config_.VerboseLogging()) {
          LOG(INFO) << "Pod: " << *raw_pod;
        }
        const json::Object* pod = raw_pod->As<json::Object>();

        const json::Object* metadata = pod->Get<json::Object>("metadata");
        const std::string pod_name = metadata->Get<json::String>("name");
        const std::string pod_id = metadata->Get<json::String>("uid");

        const json::Object* spec = pod->Get<json::Object>("spec");

        if (!node_name.empty()) {
          const std::string pod_node_name = spec->Get<json::String>("nodeName");
          if (pod_node_name != node_name) {
            LOG(ERROR) << "Internal error; pod's node " << pod_node_name
                       << " not the same as agent node " << node_name;
          }
        }

        const json::Object* status = pod->Get<json::Object>("status");

        const json::Array* container_specs = spec->Get<json::Array>("containers");
        if (status->Has("containerStatuses")) {
          const json::Array* container_statuses =
              status->Get<json::Array>("containerStatuses");
          if (container_specs->size() != container_statuses->size()) {
            LOG(ERROR) << "Container specs and statuses arrays "
                       << "have different sizes: "
                       << container_specs->size() << " vs "
                       << container_statuses->size() << " for pod "
                       << pod_id << "(" << pod_name << ")";
          }
        } else {
          LOG(INFO) << "Container statuses do not exist in status for pod "
                    << pod_id << "(" << pod_name << ")";
        }

        // TODO: find is_deleted.
        //const json::Object* status = pod->Get<json::Object>("status");
        bool is_deleted = false;
        std::vector<MetadataUpdater::ResourceMetadata> full_pod_metadata =
            GetPodAndContainerMetadata(pod, collected_at, is_deleted);
        for (MetadataUpdater::ResourceMetadata& metadata : full_pod_metadata) {
          result.emplace_back(std::move(metadata));
        }
      } catch (const json::Exception& e) {
        LOG(ERROR) << e.what();
        continue;
      }
    }
  } catch (const json::Exception& e) {
    LOG(ERROR) << e.what();
  } catch (const QueryException& e) {
    // Already logged.
  }
  return result;
}

json::value KubernetesReader::QueryMaster(const std::string& path) const
    throw(QueryException, json::Exception) {
  const std::string endpoint(config_.KubernetesEndpointHost() + path);
  http::client client(
      http::client::options()
      .openssl_certificate(SecretPath("ca.crt")));
  http::client::request request(endpoint);
  request << boost::network::header(
      "Authorization", "Bearer " + KubernetesApiToken());
  if (config_.VerboseLogging()) {
    LOG(INFO) << "QueryMaster: Contacting " << endpoint;
  }
  try {
    http::client::response response = client.get(request);
    if (status(response) >= 400 && status(response) <= 403) {
      const std::string what =
          format::Substitute("Server responded with '{{message}}' ({{code}})",
                             {{"message", status_message(response)},
                              {"code", format::str(status(response))}});
      LOG(ERROR) << "Failed to query " << endpoint << ": " << what;
      throw NonRetriableError(what);
    } else if (status(response) >= 300) {
      throw boost::system::system_error(
          boost::system::errc::make_error_code(boost::system::errc::not_connected),
          format::Substitute("Server responded with '{{message}}' ({{code}})",
                             {{"message", status_message(response)},
                              {"code", format::str(status(response))}}));
    }
#ifdef VERBOSE
    LOG(DEBUG) << "QueryMaster: Response: " << body(response);
#endif
    return json::Parser::FromString(body(response));
  } catch (const boost::system::system_error& e) {
    LOG(ERROR) << "Failed to query " << endpoint << ": " << e.what();
    throw QueryException(endpoint + " -> " + e.what());
  }
}

const std::string KubernetesReader::ClusterFullName() const {
  const std::string project_id = environment_.NumericProjectId();
  const std::string cluster_name = environment_.KubernetesClusterName();
  const std::string location = environment_.KubernetesClusterLocation();
  const std::string location_type =
      environment_.IsGcpLocationZonal(location) ? "zones": "locations";
  return format::Substitute(kClusterFullNameFormat,
                            {{"project_id", project_id},
                             {"location_type", location_type},
                             {"location", location},
                             {"cluster_name", cluster_name}});
}

const std::string KubernetesReader::FullResourceName(
    const std::string& self_link) const {
  std::vector<std::string> slash_split;
  boost::algorithm::split(
      slash_split, self_link, boost::algorithm::is_any_of("/"));
  if (slash_split.size() < 5 ||
      (slash_split[1] != "api" && slash_split[1] != "apis")) {
    LOG(ERROR) << "Invalid selfLink: " << self_link;
    assert(false);
  }

  std::vector<std::string> link_components;
  // The logic here gets rid of the leading "/api" or "/apis" along with the
  // resource version.
  if (slash_split[1] == "api") {
    // Core resources start with "/api/<version>/...".
    link_components.assign(slash_split.begin() + 3, slash_split.end());
  } else {
    // Non-core resources start with "/apis/<group-name>/<version>/...".
    // The <group-name> is part of the resource type, and hence the full name.
    const std::string group_name = slash_split[2];
    link_components.assign(slash_split.begin() + 4, slash_split.end());
    // group name needs to go before the type. E.g. ".../deployments/dep".
    link_components.insert(link_components.end() - 2, group_name);
  }
  const std::string relative_link =
      boost::algorithm::join(link_components, "/");
  return format::Substitute(kK8sFullNameFormat,
                            {{"cluster_full_name", ClusterFullName()},
                             {"relative_link", relative_link}});
}

namespace {
struct Watcher {
  Watcher(const std::string& endpoint,
          std::function<void(json::value)> event_callback,
          std::unique_lock<std::mutex>&& completion, bool verbose)
      : name_("Watcher(" + endpoint + ")"),
        completion_(std::move(completion)), event_parser_(event_callback),
        verbose_(verbose) {}
  ~Watcher() {}  // Unlocks the completion_ lock.

 public:
  void operator()(const boost::iterator_range<const char*>& range,
                  const boost::system::error_code& error) {
    const std::string body(std::begin(range), std::end(range));
    if (!body.empty()) {
#ifdef VERBOSE
        LOG(DEBUG) << name_ << " => Parsing '" << body << "'";
#endif
      try {
        std::istringstream input(body);
        event_parser_.ParseStream(input);
      } catch (const json::Exception& e) {
        LOG(ERROR) << "Unable to process events: " << e.what();
      }
    } else if (!error) {
#ifdef VERBOSE
      LOG(DEBUG) << name_ << " => Skipping empty watch notification";
#endif
    }
    if (error) {
      if (error == boost::asio::error::eof) {
#ifdef VERBOSE
        LOG(DEBUG) << name_ << " => Watch callback: EOF";
#endif
        try {
          event_parser_.NotifyEOF();
        } catch (const json::Exception& e) {
          LOG(ERROR) << "Error while processing last event: " << e.what();
        }
      } else {
        LOG(ERROR) << name_ << " => Callback got error " << error;
      }
      if (verbose_) {
        LOG(ERROR) << name_ << " => Unlocking completion mutex";
      }
      completion_.unlock();
    }
  }

 private:
  std::string name_;
  std::unique_lock<std::mutex> completion_;
  json::Parser event_parser_;
  bool verbose_;
};

void WatchEventCallback(
    std::function<void(const json::Object*, Timestamp, bool)> callback,
    const std::string& name, json::value raw_watch) throw(json::Exception) {
  Timestamp collected_at = std::chrono::system_clock::now();

#ifdef VERBOSE
  LOG(DEBUG) << name << " => WatchEventCallback('" << *raw_watch << "')";
#endif
  const json::Object* watch = raw_watch->As<json::Object>();
  const std::string type = watch->Get<json::String>("type");
  const json::Object* object = watch->Get<json::Object>("object");
#ifdef VERBOSE
  LOG(DEBUG) << "Watch type: " << type << " object: " << *object;
#endif
  if (type != "MODIFIED" && type != "ADDED" && type != "DELETED") {
    return;
  }
  const bool is_deleted = (type == "DELETED");
  callback(object, collected_at, is_deleted);
}
}

void KubernetesReader::WatchMaster(
    const std::string& name,
    const std::string& path,
    std::function<void(const json::Object*, Timestamp, bool)> callback) const
    throw(QueryException, json::Exception) {
  const std::string endpoint(config_.KubernetesEndpointHost() + path);
  http::client client(
      http::client::options()
      .openssl_certificate(SecretPath("ca.crt")));
  http::client::request request(endpoint);
  request << boost::network::header(
      "Authorization", "Bearer " + KubernetesApiToken());
  const bool verbose = config_.VerboseLogging();
  int failures = 0;
  while (true) {
    if (verbose) {
      if (failures > 0) {
        LOG(INFO) << "WatchMaster(" << name
                  << "): Retrying; attempt #" << failures
                  << " of " << config_.KubernetesUpdaterWatchConnectionRetries();
      }
      LOG(INFO) << "WatchMaster(" << name << "): Contacting " << endpoint;
    }
    try {
      if (verbose) {
        LOG(INFO) << "Locking completion mutex";
      }
      // A notification for watch completion.
      std::mutex completion_mutex;
      std::unique_lock<std::mutex> watch_completion(completion_mutex);
      Watcher watcher(
        endpoint,
        [=](json::value raw_watch) {
          WatchEventCallback(callback, name, std::move(raw_watch));
        },
        std::move(watch_completion), verbose);
      http::client::response response = client.get(request, std::ref(watcher));
      if (verbose) {
        LOG(INFO) << "Waiting for completion";
      }
      std::lock_guard<std::mutex> await_completion(completion_mutex);
      if (verbose) {
        LOG(INFO) << "WatchMaster(" << name << ") completed " << body(response);
      }
      // Connection closed without an error; reset failure count.
      failures = 0;
    } catch (const boost::system::system_error& e) {
      LOG(ERROR) << "Failed to query " << endpoint << ": " << e.what();
      ++failures;
      if (failures >= config_.KubernetesUpdaterWatchConnectionRetries()) {
        LOG(ERROR) << "WatchMaster(" << name << "): Exiting after "
                   << failures << " retries";
        throw QueryException(endpoint + " -> " + e.what());
      }
      double backoff = fmin(pow(1.5, failures), 30);
      if (verbose) {
        LOG(INFO) << "Backing off for " << backoff << " seconds";
      }
      std::this_thread::sleep_for(time::seconds(backoff));
    }
  }
}

const std::string& KubernetesReader::KubernetesApiToken() const {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  if (kubernetes_api_token_.empty()) {
    if (!ReadServiceAccountSecret("token", kubernetes_api_token_,
                                  config_.VerboseLogging())) {
      LOG(ERROR) << "Failed to read Kubernetes API token";
    }
  }
  return kubernetes_api_token_;
}

const std::string& KubernetesReader::KubernetesNamespace() const {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  if (kubernetes_namespace_.empty()) {
    if (!ReadServiceAccountSecret("namespace", kubernetes_namespace_,
                                  config_.VerboseLogging())) {
      LOG(ERROR) << "Failed to read Kubernetes namespace";
    }
  }
  return kubernetes_namespace_;
}

const std::string& KubernetesReader::CurrentNode() const {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  if (current_node_.empty()) {
    if (!config_.KubernetesNodeName().empty()) {
      current_node_ = config_.KubernetesNodeName();
    } else {
      const std::string& ns = KubernetesNamespace();
      // TODO: This is unreliable, see
      // https://github.com/kubernetes/kubernetes/issues/52162.
      const std::string pod_name = boost::asio::ip::host_name();
      try {
        json::value pod_response = QueryMaster(
            std::string(kKubernetesEndpointPath) +
            "/namespaces/" + ns + "/pods/" + pod_name);
        const json::Object* pod = pod_response->As<json::Object>();
        const json::Object* spec = pod->Get<json::Object>("spec");
        current_node_ = spec->Get<json::String>("nodeName");
      } catch (const json::Exception& e) {
        LOG(ERROR) << e.what();
      } catch (const QueryException& e) {
        // Already logged.
      }
    }
  }
  return current_node_;
}

const std::string KubernetesReader::GetWatchPath(
    const std::string& plural_kind, const std::string& api_version,
    const std::string& selector) const {
  bool has_slash = api_version.find('/') != std::string::npos;
  const std::string api_prefix = has_slash ? "apis" : "api";
  return format::Substitute(kK8sWatchPathFormat,
                            {{"api_prefix", api_prefix},
                             {"api_version", api_version},
                             {"plural_kind", plural_kind},
                             {"selector", selector}});
}

void KubernetesReader::ValidateDynamicConfiguration() const
    throw(MetadataUpdater::ConfigurationValidationError) {
  try {
    util::Retry<NonRetriableError, QueryException>(
        kKubernetesValidationRetryLimit,
        time::seconds(kKubernetesValidationRetryDelaySeconds),
        [this]() {
          (void) QueryMaster(std::string(kKubernetesEndpointPath)
                             + "/nodes?limit=1");
        });
  } catch (const NonRetriableError& e) {
    throw MetadataUpdater::ConfigurationValidationError(
        "Node query validation failed: " + e.what());
  } catch (const QueryException& e) {
    // Already logged.
    throw MetadataUpdater::ConfigurationValidationError(
        "Node query validation retry limit reached: " + e.what());
  }

  const std::string pod_label_selector(
      config_.KubernetesPodLabelSelector().empty()
      ? "" : "&" + config_.KubernetesPodLabelSelector());

  try {
    util::Retry<NonRetriableError, QueryException>(
      kKubernetesValidationRetryLimit,
      time::seconds(kKubernetesValidationRetryDelaySeconds),
      [this, &pod_label_selector]() {
        (void) QueryMaster(std::string(kKubernetesEndpointPath)
                           + "/pods?limit=1" + pod_label_selector);
      });
  } catch (const NonRetriableError& e) {
    throw MetadataUpdater::ConfigurationValidationError(
        "Pod query validation failed: " + e.what());
  } catch (const QueryException& e) {
    // Already logged.
    throw MetadataUpdater::ConfigurationValidationError(
        "Pod query validation retry limit reached: " + e.what());
  }

  if (CurrentNode().empty()) {
    throw new MetadataUpdater::ConfigurationValidationError(
        "Current node cannot be empty");
  }
}

void KubernetesReader::WatchPods(
    const std::string& node_name,
    MetadataUpdater::UpdateCallback callback) const {
  const std::string node_selector(kNodeSelectorPrefix + node_name);
  const std::string pod_label_selector(
      config_.KubernetesPodLabelSelector().empty()
      ? "" : "&" + config_.KubernetesPodLabelSelector());
  WatchEndpoint(
      "pods",
      GetWatchPath("pods", "v1", node_selector + pod_label_selector),
      [=](const json::Object* pod) {
        return PodResourceMappingCallback(pod);
      },
      callback);
}

void KubernetesReader::WatchNodes(
    const std::string& node_name,
    MetadataUpdater::UpdateCallback callback) const {
  WatchEndpoint(
      "nodes", GetWatchPath("nodes", "v1", "/" + node_name),
      [=](const json::Object* node) {
        return NodeResourceMappingCallback(node);
      },
      callback);
}

void KubernetesReader::WatchObjects(
    const std::string& plural_kind, const std::string& api_version,
    MetadataUpdater::UpdateCallback callback) const {
  WatchEndpoint(
      plural_kind,
      GetWatchPath(plural_kind, api_version, /*selector=*/""),
      [=](const json::Object* object) {
        return EmptyResourceMappingCallback(object);
      },
      callback);
}

void KubernetesReader::ObjectWatchCallback(
    KubernetesReader::ResourceMappingCallback resource_mapping_cb,
    MetadataUpdater::UpdateCallback update_cb,
    const json::Object* object, Timestamp collected_at, bool is_deleted) const
    throw(json::Exception) {
  std::vector<MetadataUpdater::ResourceMetadata> result_vector;
  result_vector.emplace_back(
      GetObjectMetadata(object, collected_at, is_deleted, resource_mapping_cb));
  update_cb(std::move(result_vector));
}

void KubernetesReader::WatchEndpoint(
    const std::string& name, const std::string& path,
    KubernetesReader::ResourceMappingCallback resource_mapping_cb,
    MetadataUpdater::UpdateCallback update_cb) const {
  LOG(INFO) << "Watch thread (" << name << ") started";
  try {
    WatchMaster(
        name, path,
        [=](const json::Object* object, Timestamp collected_at,
            bool is_deleted) {
          ObjectWatchCallback(
              resource_mapping_cb, update_cb, object, collected_at, is_deleted);
        });
  } catch (const json::Exception& e) {
    LOG(ERROR) << e.what();
    LOG(ERROR) << "No more " << name << " metadata will be collected";
  } catch (const KubernetesReader::QueryException& e) {
    LOG(ERROR) << "No more " << name << " metadata will be collected";
  }
  if (health_checker_) {
    health_checker_->SetUnhealthy("kubernetes_" + name + "_thread");
  }
  LOG(INFO) << "Watch thread (" << name << ") exiting";
}

KubernetesUpdater::KubernetesUpdater(const Configuration& config,
                                     HealthChecker* health_checker,
                                     MetadataStore* store)
    : reader_(config, health_checker), PollingMetadataUpdater(
        config, store, "KubernetesUpdater",
        config.KubernetesUpdaterIntervalSeconds(),
        [=]() { return reader_.MetadataQuery(); }) { }

void KubernetesUpdater::ValidateDynamicConfiguration() const
    throw(ConfigurationValidationError) {
  PollingMetadataUpdater::ValidateDynamicConfiguration();
  reader_.ValidateDynamicConfiguration();
}

bool KubernetesUpdater::ShouldStartUpdater() const {
  return
      PollingMetadataUpdater::ShouldStartUpdater() ||
      config().KubernetesUseWatch();
}

void KubernetesUpdater::StartUpdater() {
  if (config().KubernetesUseWatch()) {
    const std::string current_node = reader_.CurrentNode();

    if (config().VerboseLogging()) {
      LOG(INFO) << "Current node is " << current_node;
    }

    if (config().KubernetesClusterLevelMetadata()) {
      LOG(INFO) << "Watching for cluster-level metadata";
    } else {
      LOG(INFO) << "Watching for node-level metadata";
    }

    const std::string watched_node(
        config().KubernetesClusterLevelMetadata() ? "" : current_node);

    auto cb = [=](std::vector<MetadataUpdater::ResourceMetadata>&& results) {
      MetadataCallback(std::move(results));
    };
    node_watch_thread_ = std::thread([=]() {
      reader_.WatchNodes(watched_node, cb);
    });
    pod_watch_thread_ = std::thread([=]() {
      reader_.WatchPods(watched_node, cb);
    });
    if (config().KubernetesClusterLevelMetadata() &&
        config().KubernetesServiceMetadata()) {
      service_watch_thread_ = std::thread([=]() {
        reader_.WatchObjects("services", "v1", cb);
      });
      endpoints_watch_thread_ = std::thread([=]() {
        reader_.WatchObjects("endpoints", "v1", cb);
      });
    }
  } else {
    // Only try to poll if watch is disabled.
    PollingMetadataUpdater::StartUpdater();
  }
}

void KubernetesUpdater::NotifyStopUpdater() {
  if (config().KubernetesUseWatch()) {
    // TODO: How do we interrupt a watch thread?
  } else {
    // Only stop polling if watch is disabled.
    PollingMetadataUpdater::NotifyStopUpdater();
  }
}

void KubernetesUpdater::MetadataCallback(
    std::vector<MetadataUpdater::ResourceMetadata>&& result_vector) {
  for (MetadataUpdater::ResourceMetadata& result : result_vector) {
    UpdateResourceCallback(result);
    UpdateMetadataCallback(std::move(result));
  }
}

}
