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
#include "logging.h"
#include "resource.h"
#include "store.h"
#include "util.h"

namespace http = boost::network::http;

namespace google {

namespace {

constexpr const int kKubernetesValidationRetryLimit = 10;
constexpr const int kKubernetesValidationRetryDelaySeconds = 1;

#if 0
constexpr const char kKubernetesEndpointHost[] = "https://kubernetes.default.svc";
#endif
constexpr const char kKubernetesApiVersion[] = "1.6";
constexpr const char kKubernetesEndpointPath[] = "/api/v1";

constexpr const char kGkeContainerResourcePrefix[] = "gke_container";
constexpr const char kK8sContainerResourcePrefix[] = "k8s_container";
constexpr const char kK8sPodResourcePrefix[] = "k8s_pod";
constexpr const char kK8sNodeResourcePrefix[] = "k8s_node";

constexpr const char kNodeSelectorPrefix[] = "?fieldSelector=spec.nodeName%3D";

constexpr const char kWatchParam[] = "watch=true";

constexpr const char kDockerIdPrefix[] = "docker://";

constexpr const char kServiceAccountDirectory[] =
    "/var/run/secrets/kubernetes.io/serviceaccount";

}

// A subclass of QueryException to represent non-retriable errors.
class KubernetesReader::NonRetriableError
    : public KubernetesReader::QueryException {
 public:
  NonRetriableError(const std::string& what) : QueryException(what) {}
};

KubernetesReader::KubernetesReader(const Configuration& config,
                                   HealthChecker* health_checker)
    : KubernetesReader(
          config, health_checker,
          DelayTimerFactoryImpl<std::chrono::steady_clock>::New()) {}

KubernetesReader::KubernetesReader(
    const Configuration& config,
    HealthChecker* health_checker,
    std::unique_ptr<DelayTimerFactory> delay_timer_factory)
    : config_(config), environment_(config), health_checker_(health_checker),
      service_account_directory_(kServiceAccountDirectory),
      delay_timer_factory_(std::move(delay_timer_factory)) {}

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

MetadataUpdater::ResourceMetadata KubernetesReader::GetNodeMetadata(
    const json::Object* node, Timestamp collected_at, bool is_deleted) const
    throw(json::Exception) {
  const std::string cluster_name = environment_.KubernetesClusterName();
  const std::string location = environment_.KubernetesClusterLocation();

  const json::Object* metadata = node->Get<json::Object>("metadata");
  const std::string node_name = metadata->Get<json::String>("name");
  const std::string created_str =
      metadata->Get<json::String>("creationTimestamp");
  Timestamp created_at = time::rfc3339::FromString(created_str);

  const MonitoredResource k8s_node("k8s_node", {
    {"cluster_name", cluster_name},
    {"node_name", node_name},
    {"location", location},
  });

  json::value associations;
  try {
    associations = json::object({
      {"version", json::string(config_.MetadataIngestionRawContentVersion())},
      {"raw", json::object({
        {"infrastructureResource",
         InstanceReader::InstanceResource(environment_).ToJSON()},
      })},
    });
  } catch (const std::out_of_range& e) {
    // No instance resource; proceed.
  }

  json::value node_raw_metadata = json::object({
    {"blobs", json::object({
      {"association", std::move(associations)},
      {"api", json::object({
        {"version", json::string(kKubernetesApiVersion)},
        {"raw", node->Clone()},
      })},
    })},
  });
  if (config_.VerboseLogging()) {
    LOG(INFO) << "Raw node metadata: " << *node_raw_metadata;
  }

  const std::string k8s_node_name = boost::algorithm::join(
      std::vector<std::string>{kK8sNodeResourcePrefix, node_name},
      config_.MetadataApiResourceTypeSeparator());
  return MetadataUpdater::ResourceMetadata(
      std::vector<std::string>{k8s_node_name},
      k8s_node,
      MetadataStore::Metadata(config_.MetadataIngestionRawContentVersion(),
                              is_deleted, created_at, collected_at,
                              std::move(node_raw_metadata))
  );
}

json::value KubernetesReader::ComputePodAssociations(const json::Object* pod)
    const throw(json::Exception) {
  const json::Object* metadata = pod->Get<json::Object>("metadata");
  const std::string namespace_name = metadata->Get<json::String>("namespace");

  json::value instance_resource;
  try {
    instance_resource =
        InstanceReader::InstanceResource(environment_).ToJSON();
  } catch (const std::out_of_range& e) {
    // No instance resource; proceed.
  }

  json::value controllers;
  try {
    const json::value top_level = FindTopLevelController(
        namespace_name, pod->Clone());
    const json::Object* top_level_controller = top_level->As<json::Object>();
    const json::Object* top_level_metadata =
        top_level_controller->Get<json::Object>("metadata");
    const std::string top_level_name =
        top_level_metadata->Get<json::String>("name");
    const std::string pod_id = metadata->Get<json::String>("uid");
    if (!top_level_controller->Has("kind") &&
        top_level_metadata->Get<json::String>("uid") != pod_id) {
      LOG(ERROR) << "Internal error; top-level controller without 'kind' "
                 << *top_level_controller
                 << " not the same as pod " << *pod;
    }
    const std::string top_level_kind =
        top_level_controller->Has("kind")
            ? top_level_controller->Get<json::String>("kind")
            : "Pod";

    controllers = json::object({
      {"topLevelControllerType", json::string(top_level_kind)},
      {"topLevelControllerName", json::string(top_level_name)},
    });
  } catch (const QueryException& e) {
    LOG(ERROR) << "Error while finding top-level controller for "
               << namespace_name << "." << metadata->Get<json::String>("name")
               << ": " << e.what();
  }

  json::value node_name;
  const json::Object* spec = pod->Get<json::Object>("spec");
  if (spec->Has("nodeName")) {
    // Pods that have been scheduled will have a nodeName.
    node_name = json::string(spec->Get<json::String>("nodeName"));
  }

  // If instance_resource, controllers, or node_name are not populated,
  // they will be discarded.
  json::value raw_associations = json::object({
    {"infrastructureResource", std::move(instance_resource)},
    {"controllers", std::move(controllers)},
    {"nodeName", std::move(node_name)},
  });

  // If none of the field values above were populated, discard the object.
  if (raw_associations->As<json::Object>()->empty()) {
    return json::value();
  }

  return json::object({
    {"version", json::string(config_.MetadataIngestionRawContentVersion())},
    {"raw", std::move(raw_associations)},
  });
}

MetadataUpdater::ResourceMetadata KubernetesReader::GetPodMetadata(
    const json::Object* pod, json::value associations, Timestamp collected_at,
    bool is_deleted) const throw(json::Exception) {
  const std::string cluster_name = environment_.KubernetesClusterName();
  const std::string location = environment_.KubernetesClusterLocation();

  const json::Object* metadata = pod->Get<json::Object>("metadata");
  const std::string namespace_name = metadata->Get<json::String>("namespace");
  const std::string pod_name = metadata->Get<json::String>("name");
  const std::string pod_id = metadata->Get<json::String>("uid");
  const std::string created_str =
      metadata->Get<json::String>("creationTimestamp");
  Timestamp created_at = time::rfc3339::FromString(created_str);

  const MonitoredResource k8s_pod("k8s_pod", {
    {"cluster_name", cluster_name},
    {"namespace_name", namespace_name},
    {"pod_name", pod_name},
    {"location", location},
  });

  json::value pod_raw_metadata = json::object({
    {"blobs", json::object({
      {"association", std::move(associations)},
      {"api", json::object({
        {"version", json::string(kKubernetesApiVersion)},
        {"raw", pod->Clone()},
      })},
    })},
  });
  if (config_.VerboseLogging()) {
    LOG(INFO) << "Raw pod metadata: " << *pod_raw_metadata;
  }

  const std::string k8s_pod_id = boost::algorithm::join(
      std::vector<std::string>{kK8sPodResourcePrefix, pod_id},
      config_.MetadataApiResourceTypeSeparator());
  const std::string k8s_pod_name = boost::algorithm::join(
      std::vector<std::string>{kK8sPodResourcePrefix, namespace_name, pod_name},
      config_.MetadataApiResourceTypeSeparator());
  return MetadataUpdater::ResourceMetadata(
      std::vector<std::string>{k8s_pod_id, k8s_pod_name},
      k8s_pod,
      MetadataStore::Metadata(config_.MetadataIngestionRawContentVersion(),
                              is_deleted, created_at, collected_at,
                              std::move(pod_raw_metadata))
  );
}

MetadataUpdater::ResourceMetadata KubernetesReader::GetContainerMetadata(
    const json::Object* pod, const json::Object* container_spec,
    const json::Object* container_status, json::value associations,
    Timestamp collected_at, bool is_deleted) const throw(json::Exception) {
  const std::string cluster_name = environment_.KubernetesClusterName();
  const std::string location = environment_.KubernetesClusterLocation();

  const json::Object* metadata = pod->Get<json::Object>("metadata");
  const std::string namespace_name = metadata->Get<json::String>("namespace");
  const std::string pod_name = metadata->Get<json::String>("name");
  const std::string pod_id = metadata->Get<json::String>("uid");
  const std::string created_str =
      metadata->Get<json::String>("creationTimestamp");
  Timestamp created_at = time::rfc3339::FromString(created_str);
  const json::Object* labels;
  if (!metadata->Has("labels")) {
    labels = nullptr;
  } else {
    labels = metadata->Get<json::Object>("labels");
  }

  const std::string container_name = container_spec->Get<json::String>("name");

  const MonitoredResource k8s_container("k8s_container", {
    {"cluster_name", cluster_name},
    {"namespace_name", namespace_name},
    {"pod_name", pod_name},
    {"container_name", container_name},
    {"location", location},
  });

  json::value raw_status;
  if (container_status) {
    raw_status = json::object({
      {"version", json::string(kKubernetesApiVersion)},
      {"raw", container_status->Clone()},
    });
  }

  json::value raw_labels;
  if (labels) {
    raw_labels = json::object({
      {"version", json::string(kKubernetesApiVersion)},
      {"raw", labels->Clone()},
    });
  }
  json::value container_raw_metadata = json::object({
    {"blobs", json::object({
      {"association", std::move(associations)},
      {"spec", json::object({
        {"version", json::string(kKubernetesApiVersion)},
        {"raw", container_spec->Clone()},
      })},
      {"status", std::move(raw_status)},
      {"labels", std::move(raw_labels)},
    })},
  });
  if (config_.VerboseLogging()) {
    LOG(INFO) << "Raw container metadata: " << *container_raw_metadata;
  }

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
      MetadataStore::Metadata(kKubernetesApiVersion,
                              is_deleted, created_at, collected_at,
                              std::move(container_raw_metadata))
  );
}

MetadataUpdater::ResourceMetadata KubernetesReader::GetLegacyResource(
    const json::Object* pod, const std::string& container_name) const
    throw(std::out_of_range, json::Exception) {
  const std::string instance_id = environment_.InstanceId();
  const std::string zone = environment_.InstanceZone();
  const std::string cluster_name = environment_.KubernetesClusterName();

  if (instance_id.empty() || zone.empty()) {
    throw std::out_of_range("No instance information, skipping gke_container");
  }

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

  json::value associations = ComputePodAssociations(pod);

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
    try {
      result.emplace_back(GetLegacyResource(pod, name));
    } catch (const std::out_of_range& e) {
      // No instance information available; log and ignore.
      LOG(INFO) << e.what();
    }
    result.emplace_back(
        GetContainerMetadata(pod, container_spec, container_status,
                             json::Clone(associations), collected_at,
                             is_deleted));
  }

  result.emplace_back(
      GetPodMetadata(pod, std::move(associations), collected_at, is_deleted));
  return std::move(result);
}

std::vector<json::value> KubernetesReader::GetServiceList(
    const std::string& cluster_name, const std::string& location) const
    throw(json::Exception) {
  std::lock_guard<std::mutex> lock(service_mutex_);
  std::vector<json::value> service_list;
  for (const auto& metadata_it : service_to_metadata_) {
    // A service key consists of a namespace name and a service name.
    const ServiceKey& service_key = metadata_it.first;
    const std::string namespace_name = service_key.first;
    const json::value& service_metadata = metadata_it.second;
    auto pods_it = service_to_pods_.find(service_key);
    const std::vector<std::string>& pod_names =
        (pods_it != service_to_pods_.end()) ? pods_it->second : kNoPods;
    std::vector<json::value> pod_resources;
    for (const std::string& pod_name : pod_names) {
      const MonitoredResource k8s_pod("k8s_pod", {
        {"cluster_name", cluster_name},
        {"namespace_name", namespace_name},
        {"pod_name", pod_name},
        {"location", location},
      });
      pod_resources.emplace_back(k8s_pod.ToJSON());
    }
    service_list.emplace_back(json::object({
      {"api", json::object({
        {"version", json::string(kKubernetesApiVersion)},
        {"raw", service_metadata->Clone()},
        {"pods", json::array(std::move(pod_resources))},
      })},
    }));
  }
  return service_list;
}

MetadataUpdater::ResourceMetadata KubernetesReader::GetClusterMetadata(
    Timestamp collected_at) const throw(json::Exception) {
  const std::string cluster_name = environment_.KubernetesClusterName();
  const std::string location = environment_.KubernetesClusterLocation();
  std::vector<json::value> service_list =
      GetServiceList(cluster_name, location);
  const MonitoredResource k8s_cluster("k8s_cluster", {
    {"cluster_name", cluster_name},
    {"location", location},
  });
  json::value cluster_raw_metadata = json::object({
    {"blobs", json::object({
      {"services", json::array(std::move(service_list))},
    })},
  });
  if (config_.VerboseLogging()) {
    LOG(INFO) << "Raw cluster metadata: " << *cluster_raw_metadata;
  }

  // There is no created_at for the cluster since the metadata contains
  // ALL current services.
  Timestamp created_at = time_point();
  return MetadataUpdater::ResourceMetadata(
      std::vector<std::string>{},
      k8s_cluster,
      MetadataStore::Metadata(config_.MetadataIngestionRawContentVersion(),
                              /*is_deleted=*/false, created_at, collected_at,
                              std::move(cluster_raw_metadata))
  );
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
    for (const json::value& raw_node : *nodes_array) {
      const json::Object* node = raw_node->As<json::Object>();
      result.emplace_back(
          GetNodeMetadata(node, collected_at, /*is_deleted=*/false));
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

namespace {
struct Watcher : public std::thread {
  Watcher(const std::string& endpoint,
          std::function<void(json::value)> event_callback,
          std::shared_ptr<boost::asio::io_service> service, bool verbose)
      : std::thread([=]() { service->run(); }),
        name_("Watcher(" + endpoint + ")"), service_(service),
        event_parser_(event_callback), verbose_(verbose) {}
  ~Watcher() {}

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
      service_->stop();
    }
  }

 private:
  std::string name_;
  std::shared_ptr<boost::asio::io_service> service_;
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
  const std::string prefix((path.find('?') == std::string::npos) ? "?" : "&");
  const std::string watch_param(prefix + kWatchParam);
  const std::string endpoint(
      config_.KubernetesEndpointHost() + path + watch_param);
  auto watch_service = std::make_shared<boost::asio::io_service>();
  http::client client(
      http::client::options()
      .openssl_certificate(SecretPath("ca.crt"))
      .io_service(watch_service));
  http::client::request request(endpoint);
  request << boost::network::header(
      "Authorization", "Bearer " + KubernetesApiToken());
  const bool verbose = config_.VerboseLogging();
  const int retries = config_.KubernetesUpdaterWatchConnectionRetries();
  int failures = 0;
  for (int i = 0; retries <= 0 || i < retries; i++) {
    if (verbose) {
      if (failures > 0) {
        LOG(INFO) << "WatchMaster(" << name
                  << "): Retrying; attempt #" << failures
                  << " of " << config_.KubernetesUpdaterWatchMaxConnectionFailures();
      }
      LOG(INFO) << "WatchMaster(" << name << "): Contacting " << endpoint;
    }
    try {
      if (verbose) {
        LOG(INFO) << "Resetting and running service";
      }
      if (watch_service->stopped()) {
        watch_service->reset();
      }
      std::unique_ptr<DelayTimer> timer;
      if (config_.KubernetesUpdaterWatchReconnectIntervalSeconds() > 0) {
        timer = delay_timer_factory_->CreateTimer(*watch_service);
        timer->RunAsyncAfter(std::chrono::seconds(
            config_.KubernetesUpdaterWatchReconnectIntervalSeconds()),
            [watch_service](boost::system::error_code const &ec) {
          if (ec != boost::asio::error::operation_aborted) {
            watch_service->stop();
          }
        });
      }
      Watcher watcher(
        endpoint,
        [=](json::value raw_watch) {
          WatchEventCallback(callback, name, std::move(raw_watch));
        },
        watch_service, verbose);
      http::client::response response = client.get(request, std::ref(watcher));
      if (verbose) {
        LOG(INFO) << "Waiting for completion";
      }
      watcher.join();
      if (verbose) {
        LOG(INFO) << "WatchMaster(" << name << ") completed " << body(response);
      }
      // Connection closed without an error; reset failure count.
      failures = 0;
    } catch (const boost::system::system_error& e) {
      LOG(ERROR) << "Failed to query " << endpoint << ": " << e.what();
      ++failures;
      if (failures > config_.KubernetesUpdaterWatchMaxConnectionFailures()) {
        LOG(ERROR) << "WatchMaster(" << name << "): Exiting after "
                   << failures << " failures";
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

std::pair<std::string, std::string> KubernetesReader::KindPath(
    const std::string& version, const std::string& kind) const {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  const std::string prefix(
      (version.find('/') == std::string::npos) ? "/api" : "/apis");
  const std::string query_path(prefix + "/" + version);
  auto found = version_to_kind_to_name_.emplace(
      std::piecewise_construct,
      std::forward_as_tuple(version),
      std::forward_as_tuple());
  std::map<std::string, std::string>& kind_to_name = found.first->second;
  if (found.second) {  // Not found, inserted new.
    try {
      json::value apilist_response = QueryMaster(query_path);
      if (config_.VerboseLogging()) {
        LOG(INFO) << "Parsed API list: " << *apilist_response;
      }

      const json::Object* apilist_object = apilist_response->As<json::Object>();
      const json::Array* api_list = apilist_object->Get<json::Array>("resources");
      for (const json::value& element : *api_list) {
        const json::Object* resource = element->As<json::Object>();
        const std::string resource_name = resource->Get<json::String>("name");
        // Hack: the API seems to return multiple names for the same kind.
        if (resource_name.find('/') != std::string::npos) {
          continue;
        }
        const std::string resource_kind = resource->Get<json::String>("kind");
        kind_to_name.emplace(resource_kind, resource_name);
      }
    } catch (const json::Exception& e) {
      LOG(ERROR) << e.what();
    } catch (const QueryException& e) {
      // Already logged.
    }
  }

  auto name_it = kind_to_name.find(kind);
  if (name_it == kind_to_name.end()) {
    throw std::string("Unknown kind: ") + kind;
  }
  return {query_path, name_it->second};
}

json::value KubernetesReader::GetOwner(
    const std::string& ns, const json::Object* owner_ref) const
    throw(QueryException, json::Exception) {
#ifdef VERBOSE
  LOG(DEBUG) << "GetOwner(" << ns << ", " << *owner_ref << ")";
#endif
  const std::string api_version = owner_ref->Get<json::String>("apiVersion");
  const std::string kind = owner_ref->Get<json::String>("kind");
  const std::string name = owner_ref->Get<json::String>("name");
  const std::string uid = owner_ref->Get<json::String>("uid");

  // Even though we query by name, we should look up the owner by uid,
  // to handle the case when an object is deleted and re-constructed.
  const std::string encoded_ref = boost::algorithm::join(
      std::vector<std::string>{api_version, kind, uid}, "/");

  std::lock_guard<std::recursive_mutex> lock(mutex_);
  auto owner_it = owners_.find(encoded_ref);
  if (owner_it == owners_.end()) {  // Not found, add new.
    const auto path_component = KindPath(api_version, kind);
#ifdef VERBOSE
    LOG(DEBUG) << "KindPath returned {" << path_component.first << ", "
               << path_component.second << "}";
#endif
    json::value owner =
        QueryMaster(path_component.first + "/namespaces/" + ns + "/" +
                    path_component.second + "/" + name);
    // Sanity check: because we are looking up by name, the object we get
    // back might have a different uid.
    const json::Object* owner_obj = owner->As<json::Object>();
    const json::Object* metadata = owner_obj->Get<json::Object>("metadata");
    const std::string owner_uid = metadata->Get<json::String>("uid");
    if (owner_uid != uid) {
      LOG(WARNING) << "Owner " << kind << "'" << name << "' (id " << uid
                   << ") disappeared before we could query it. Found id "
                   << owner_uid << " instead.";
      throw QueryException("Owner " + kind + " " + name + " (id " + uid +
                           ") disappeared");
    }
    auto inserted = owners_.emplace(encoded_ref, std::move(owner));
    owner_it = inserted.first;
  }
  return owner_it->second->Clone();
}

json::value KubernetesReader::FindTopLevelController(
    const std::string& ns, json::value object) const
    throw(QueryException, json::Exception) {
  const json::Object* obj = object->As<json::Object>();
#ifdef VERBOSE
  LOG(DEBUG) << "Looking for the top-level owner for " << *obj;
#endif
  const json::Object* metadata = obj->Get<json::Object>("metadata");
#ifdef VERBOSE
  LOG(DEBUG) << "FindTopLevelController: metadata is " << *metadata;
#endif
  if (!metadata->Has("ownerReferences")) {
#ifdef VERBOSE
    LOG(DEBUG) << "FindTopLevelController: no owner references in "
               << *metadata;
#endif
    return object;
  }
  const json::Array* refs = metadata->Get<json::Array>("ownerReferences");
#ifdef VERBOSE
  LOG(DEBUG) << "FindTopLevelController: refs is " << *refs;
#endif

  // Kubernetes objects are supposed to have at most one controller:
  // https://kubernetes.io/docs/reference/generated/kubernetes-api/v1.10/#objectmeta-v1-meta.
  const json::Object* controller_ref = nullptr;
  for (const json::value& ref : *refs) {
    const json::Object* ref_obj = ref->As<json::Object>();
    if (ref_obj->Has("controller") &&
        ref_obj->Get<json::Boolean>("controller")) {
      controller_ref = ref_obj;
      break;
    }
  }
  if (!controller_ref) {
#ifdef VERBOSE
    LOG(DEBUG) << "FindTopLevelController: no controller references in "
               << *refs;
#endif
    return object;
  }
#ifdef VERBOSE
  LOG(DEBUG) << "FindTopLevelController: controller_ref is " << *controller_ref;
#endif
  return FindTopLevelController(ns, GetOwner(ns, controller_ref));
}

void KubernetesReader::UpdateServiceToMetadataCache(
    const json::Object* service, bool is_deleted) throw(json::Exception) {
#ifdef VERBOSE
  LOG(DEBUG) << "UpdateServiceToMetadataCache(" << *service << ")";
#endif
  const json::Object* metadata = service->Get<json::Object>("metadata");
  const std::string namespace_name = metadata->Get<json::String>("namespace");
  const std::string service_name = metadata->Get<json::String>("name");
  const ServiceKey service_key(namespace_name, service_name);

  std::lock_guard<std::mutex> lock(service_mutex_);
  if (is_deleted) {
    service_to_metadata_.erase(service_key);
  } else {
    auto it_inserted =
        service_to_metadata_.emplace(service_key, json::value());
    it_inserted.first->second = service->Clone();
  }
}

void KubernetesReader::UpdateServiceToPodsCache(
    const json::Object* endpoints, bool is_deleted) throw(json::Exception) {
#ifdef VERBOSE
  LOG(DEBUG) << "UpdateServiceToPodsCache(" << *endpoints << ")";
#endif
  const json::Object* metadata = endpoints->Get<json::Object>("metadata");
  const std::string namespace_name = metadata->Get<json::String>("namespace");
  // Endpoints name is the same as the matching service name.
  const std::string service_name = metadata->Get<json::String>("name");
  const ServiceKey service_key(namespace_name, service_name);

  std::vector<std::string> pod_names;
  // Only extract the pod names when this is not a deletion. In the case of
  // a deletion, we delete the mapping below.
  if (!is_deleted && endpoints->Has("subsets") &&
      !endpoints->at("subsets")->Is<json::Null>()) {
    const json::Array* subsets = endpoints->Get<json::Array>("subsets");
    for (const json::value& subset : *subsets) {
      const json::Object* subset_obj = subset->As<json::Object>();
      if (!subset_obj->Has("addresses")) {
        continue;
      }
      const json::Array* addresses = subset_obj->Get<json::Array>("addresses");
      for (const json::value& address : *addresses) {
        const json::Object* address_obj = address->As<json::Object>();
        if (!address_obj->Has("targetRef")) {
          continue;
        }
        const json::Object* ref = address_obj->Get<json::Object>("targetRef");
        if (!(ref->Has("kind") && ref->Has("name"))) {
          continue;
        }
        const std::string target_kind = ref->Get<json::String>("kind");
        if (target_kind != "Pod") {
          LOG(INFO) << "Found a resource other than a pod in Endpoint "
                    << service_name << "'s targetRef: " << target_kind;
          continue;
        }
        const std::string pod_name = ref->Get<json::String>("name");
        pod_names.push_back(pod_name);
      }
    }
  }

  std::lock_guard<std::mutex> lock(service_mutex_);
  if (is_deleted) {
    service_to_pods_.erase(service_key);
  } else {
    auto it_inserted =
        service_to_pods_.emplace(service_key, std::vector<std::string>());
    it_inserted.first->second = pod_names;
  }
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

void KubernetesReader::PodCallback(
    MetadataUpdater::UpdateCallback callback,
    const json::Object* pod, Timestamp collected_at, bool is_deleted) const
    throw(json::Exception) {
  std::vector<MetadataUpdater::ResourceMetadata> result_vector =
      GetPodAndContainerMetadata(pod, collected_at, is_deleted);
  callback(std::move(result_vector));
}

void KubernetesReader::WatchPods(
    const std::string& node_name,
    MetadataUpdater::UpdateCallback callback) const {
  LOG(INFO) << "Watch thread (pods) started for node "
            << (node_name.empty() ? "<unscheduled>" : node_name);

  const std::string node_selector(kNodeSelectorPrefix + node_name);
  const std::string pod_label_selector(
      config_.KubernetesPodLabelSelector().empty()
      ? "" : "&" + config_.KubernetesPodLabelSelector());

  try {
    WatchMaster(
        "Pods",
        std::string(kKubernetesEndpointPath) + "/pods" + node_selector
        + pod_label_selector,
        [=](const json::Object* pod, Timestamp collected_at, bool is_deleted) {
          PodCallback(callback, pod, collected_at, is_deleted);
        });
  } catch (const json::Exception& e) {
    LOG(ERROR) << e.what();
    LOG(ERROR) << "No more pod metadata will be collected";
  } catch (const KubernetesReader::QueryException& e) {
    LOG(ERROR) << "No more pod metadata will be collected";
  }
  if (health_checker_) {
    health_checker_->SetUnhealthy("kubernetes_pod_thread");
  }
  LOG(INFO) << "Watch thread (pods) exiting";
}

void KubernetesReader::NodeCallback(
    MetadataUpdater::UpdateCallback callback,
    const json::Object* node, Timestamp collected_at, bool is_deleted) const
    throw(json::Exception) {
  std::vector<MetadataUpdater::ResourceMetadata> result_vector;
  result_vector.emplace_back(GetNodeMetadata(node, collected_at, is_deleted));
  callback(std::move(result_vector));
}

void KubernetesReader::WatchNodes(
    const std::string& node_name,
    MetadataUpdater::UpdateCallback callback) const {
  LOG(INFO) << "Watch thread (node) started for node "
            << (node_name.empty() ? "<all>" : node_name);

  try {
    // TODO: There seems to be a Kubernetes API bug with watch=true.
    WatchMaster(
        "Node",
        std::string(kKubernetesEndpointPath) + "/watch/nodes/" + node_name,
        [=](const json::Object* node, Timestamp collected_at, bool is_deleted) {
          NodeCallback(callback, node, collected_at, is_deleted);
        });
  } catch (const json::Exception& e) {
    LOG(ERROR) << e.what();
    LOG(ERROR) << "No more node metadata will be collected";
  } catch (const KubernetesReader::QueryException& e) {
    LOG(ERROR) << "No more node metadata will be collected";
  }
  if (health_checker_) {
    health_checker_->SetUnhealthy("kubernetes_node_thread");
  }
  LOG(INFO) << "Watch thread (node) exiting";
}

void KubernetesReader::ServiceCallback(
    MetadataUpdater::UpdateCallback callback,
    const json::Object* service, Timestamp collected_at, bool is_deleted)
    throw(json::Exception) {
  UpdateServiceToMetadataCache(service, is_deleted);

  // TODO: using a temporary did not work here.
  std::vector<MetadataUpdater::ResourceMetadata> result_vector;
  result_vector.emplace_back(GetClusterMetadata(collected_at));
  callback(std::move(result_vector));
}

void KubernetesReader::WatchServices(MetadataUpdater::UpdateCallback callback) {
  LOG(INFO) << "Watch thread started for services";

  try {
    WatchMaster(
        "Service",
        std::string(kKubernetesEndpointPath) + "/watch/services/",
        [=](const json::Object* service, Timestamp collected_at,
            bool is_deleted) {
          ServiceCallback(callback, service, collected_at, is_deleted);
        });
  } catch (const json::Exception& e) {
    LOG(ERROR) << e.what();
    LOG(ERROR) << "No more service metadata will be collected";
  } catch (const KubernetesReader::QueryException& e) {
    LOG(ERROR) << "No more service metadata will be collected";
  }
  if (health_checker_) {
    health_checker_->SetUnhealthy("kubernetes_service_thread");
  }
  LOG(INFO) << "Watch thread (service) exiting";
}

void KubernetesReader::EndpointsCallback(
    MetadataUpdater::UpdateCallback callback,
    const json::Object* endpoints, Timestamp collected_at, bool is_deleted)
    throw(json::Exception) {
  UpdateServiceToPodsCache(endpoints, is_deleted);

  // TODO: using a temporary did not work here.
  std::vector<MetadataUpdater::ResourceMetadata> result_vector;
  result_vector.emplace_back(GetClusterMetadata(collected_at));
  callback(std::move(result_vector));
}

void KubernetesReader::WatchEndpoints(
    MetadataUpdater::UpdateCallback callback) {
  LOG(INFO) << "Watch thread started for endpoints";

  try {
    WatchMaster(
        "Endpoints",
        std::string(kKubernetesEndpointPath) + "/watch/endpoints/",
        [=](const json::Object* endpoints, Timestamp collected_at,
            bool is_deleted) {
          EndpointsCallback(callback, endpoints, collected_at, is_deleted);
        });
  } catch (const json::Exception& e) {
    LOG(ERROR) << e.what();
    LOG(ERROR) << "No more endpoints metadata will be collected";
  } catch (const KubernetesReader::QueryException& e) {
    LOG(ERROR) << "No more endpoints metadata will be collected";
  }
  if (health_checker_) {
    health_checker_->SetUnhealthy("kubernetes_endpoints_thread");
  }
  LOG(INFO) << "Watch thread (endpoints) exiting";
}

KubernetesUpdater::KubernetesUpdater(const Configuration& config,
                                     HealthChecker* health_checker,
                                     MetadataStore* store)
    : KubernetesUpdater(
          config, health_checker, store,
          DelayTimerFactoryImpl<std::chrono::steady_clock>::New()) {}

KubernetesUpdater::KubernetesUpdater(
    const Configuration& config,
    HealthChecker* health_checker,
    MetadataStore* store,
    std::unique_ptr<DelayTimerFactory> delay_timer_factory)
    : reader_(config, health_checker, std::move(delay_timer_factory)),
      PollingMetadataUpdater(
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
        reader_.WatchServices(cb);
      });
      endpoints_watch_thread_ = std::thread([=]() {
        reader_.WatchEndpoints(cb);
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
