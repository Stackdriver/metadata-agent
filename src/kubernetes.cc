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
#include <chrono>
#include <cstddef>
#include <fstream>
#include <tuple>

#include "json.h"
#include "logging.h"
#include "resource.h"
#include "time.h"

namespace http = boost::network::http;

namespace google {

namespace {

#if 0
constexpr const char kKubernetesEndpointHost[] = "https://kubernetes";
#endif
constexpr const char kKubernetesApiVersion[] = "1.6";
constexpr const char kKubernetesEndpointPath[] = "/api/v1";
constexpr const char kResourceTypeSeparator[] = ".";

constexpr const char kRawContentVersion[] = "0.1";

constexpr const char kGkeContainerResourcePrefix[] = "gke_container";
constexpr const char kGkeContainerNameResourcePrefix[] = "gke_containerName";
constexpr const char kK8sContainerResourcePrefix[] = "k8s_container";
constexpr const char kK8sContainerNameResourcePrefix[] = "k8s_containerName";
constexpr const char kK8sPodResourcePrefix[] = "k8s_pod";
constexpr const char kK8sPodNameResourcePrefix[] = "k8s_podName";
constexpr const char kK8sNodeResourcePrefix[] = "k8s_node";
constexpr const char kK8sNodeNameResourcePrefix[] = "k8s_nodeName";

constexpr const char kNodeSelectorPrefix[] = "?fieldSelector=spec.nodeName%3D";

constexpr const char kServiceAccountDirectory[] =
    "/var/run/secrets/kubernetes.io/serviceaccount";

// Reads a Kubernetes service account secret file into the provided string.
// Returns true if the file was read successfully.
bool ReadServiceAccountSecret(
    const std::string& secret, std::string& destination) {
  std::string filename(std::string(kServiceAccountDirectory) + "/" + secret);
  std::ifstream input(filename);
  if (!input.good()) {
    LOG(ERROR) << "Missing " << filename;
    return false;
  }
  LOG(INFO) << "Reading from " << filename;
  std::getline(input, destination);
  return !input.fail();
}

}

KubernetesReader::KubernetesReader(const MetadataAgentConfiguration& config)
    : config_(config), environment_(config) {}

std::vector<PollingMetadataUpdater::ResourceMetadata>
    KubernetesReader::MetadataQuery() const {
  LOG(INFO) << "Kubernetes Query called";
  std::vector<PollingMetadataUpdater::ResourceMetadata> result;

  const std::string platform = "gce";  // TODO: detect other platforms.
  const std::string instance_id = environment_.InstanceId();
  const std::string zone = environment_.InstanceZone();
  const std::string cluster_name = environment_.KubernetesClusterName();
  const std::string node_name = CurrentNode();

  LOG(INFO) << "Current node is " << node_name;

  const MonitoredResource k8s_node("k8s_node", {
    {"cluster_name", cluster_name},
    {"node_name", node_name},
    {"location", zone},
  });

  try {
    json::value raw_node = QueryMaster(
        std::string(kKubernetesEndpointPath) + "/nodes/" + node_name);
    Timestamp collected_at = std::chrono::system_clock::now();

    const json::Object* node = raw_node->As<json::Object>();

    const json::Object* metadata = node->Get<json::Object>("metadata");
    const std::string node_id = metadata->Get<json::String>("uid");
    const std::string created_str =
        metadata->Get<json::String>("creationTimestamp");
    Timestamp created_at = rfc3339::FromString(created_str);

    json::value node_raw_metadata = json::object({
      {"blobs", json::object({
        {"association", json::object({
          {"version", json::string(kRawContentVersion)},
          {"raw", json::object({
            {"providerPlatform", json::string(platform)},
            {"instanceId", json::string(instance_id)},
          })},
        })},
        {"api", json::object({
          {"version", json::string(kKubernetesApiVersion)},
          {"raw", std::move(raw_node)},
        })},
      })},
    });
    LOG(INFO) << "Raw node metadata: " << *node_raw_metadata;

#if 0
    // TODO: do we need this?
    const std::string k8s_node_id = boost::algorithm::join(
        std::vector<std::string>{kK8sNodeResourcePrefix, node_id},
        kResourceTypeSeparator);
#endif
    const std::string k8s_node_name = boost::algorithm::join(
        std::vector<std::string>{kK8sNodeNameResourcePrefix, node_name},
        kResourceTypeSeparator);
    result.emplace_back(
        std::vector<std::string>{k8s_node_name},
        k8s_node,
#ifdef ENABLE_KUBERNETES_METADATA
        MetadataAgent::Metadata(kRawContentVersion,
                                /*deleted=*/false, created_at, collected_at,
                                std::move(node_raw_metadata))
#else
        MetadataAgent::Metadata::IGNORED()
#endif
    );
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
    LOG(INFO) << "Parsed pod list: " << *podlist_response;
    const json::Object* podlist_object = podlist_response->As<json::Object>();
    const std::string api_version =
        podlist_object->Get<json::String>("apiVersion");
    const json::Array* pod_list = podlist_object->Get<json::Array>("items");
    for (const json::value& element : *pod_list) {
      try {
        LOG(INFO) << "Pod: " << *element;
        const json::Object* pod = element->As<json::Object>();

        const json::Object* metadata = pod->Get<json::Object>("metadata");
        const std::string namespace_name =
            metadata->Get<json::String>("namespace");
        const std::string pod_name = metadata->Get<json::String>("name");
        const std::string pod_id = metadata->Get<json::String>("uid");
        const std::string created_str =
            metadata->Get<json::String>("creationTimestamp");
        Timestamp created_at = rfc3339::FromString(created_str);
        const json::Object* labels = metadata->Get<json::Object>("labels");

        const json::Object* status = pod->Get<json::Object>("status");
        const std::string started_str = status->Get<json::String>("startTime");
        Timestamp started_at = rfc3339::FromString(started_str);

        const json::Object* spec = pod->Get<json::Object>("spec");
        const std::string pod_node_name = spec->Get<json::String>("nodeName");
        if (pod_node_name != node_name) {
          LOG(ERROR) << "Internal error; pod's node " << pod_node_name
                     << " not the same as agent node " << node_name;
        }

        const json::value primary =
            FindTopLevelOwner(namespace_name, pod->Clone());
        const json::Object* primary_controller = primary->As<json::Object>();
        const std::string primary_kind =
            primary_controller->Get<json::String>("kind");
        const json::Object* primary_metadata =
            primary_controller->Get<json::Object>("metadata");
        const std::string primary_name =
            primary_metadata->Get<json::String>("name");

        const MonitoredResource k8s_pod("k8s_pod", {
          {"cluster_name", cluster_name},
          {"namespace_name", namespace_name},
          {"node_name", node_name},
          {"pod_name", pod_name},
          {"location", zone},
        });

        // TODO: find pod_deleted.
        //const json::Object* status = pod->Get<json::Object>("status");
        bool pod_deleted = false;

        json::value associations = json::object({
          {"version", json::string(kRawContentVersion)},
          {"raw", json::object({
            {"providerPlatform", json::string(platform)},
            {"controllers", json::object({
              {"primaryControllerType", json::string(primary_kind)},
              {"primaryControllerName", json::string(primary_name)},
            })},
          })},
        });
        json::value pod_raw_metadata = json::object({
          {"blobs", json::object({
            {"association", associations->Clone()},
            {"api", json::object({
              {"version", json::string(kKubernetesApiVersion)},
              {"raw", pod->Clone()},
            })},
          })},
        });
        LOG(INFO) << "Raw pod metadata: " << *pod_raw_metadata;

        const std::string k8s_pod_id = boost::algorithm::join(
            std::vector<std::string>{kK8sPodResourcePrefix, namespace_name, pod_id},
            kResourceTypeSeparator);
        const std::string k8s_pod_name = boost::algorithm::join(
            std::vector<std::string>{kK8sPodNameResourcePrefix, namespace_name, pod_name},
            kResourceTypeSeparator);
        result.emplace_back(
            std::vector<std::string>{k8s_pod_id, k8s_pod_name},
            k8s_pod,
#ifdef ENABLE_KUBERNETES_METADATA
            MetadataAgent::Metadata(kRawContentVersion,
                                    pod_deleted, created_at, collected_at,
                                    std::move(pod_raw_metadata))
#else
            MetadataAgent::Metadata::IGNORED()
#endif
        );

        const json::Array* container_specs = spec->Get<json::Array>("containers");
        const json::Array* container_list =
            status->Get<json::Array>("containerStatuses");

        if (container_specs->size() != container_list->size()) {
          LOG(ERROR) << "Container specs and statuses arrays "
                     << "have different sizes: "
                     << container_specs->size() << " vs "
                     << container_list->size() << " for pod "
                     << pod_id << "(" << pod_name << ")";
        }
        std::size_t num_containers = std::min(
            container_list->size(), container_specs->size());

        for (int i = 0; i < num_containers; ++i) {
          const json::value& c_element = (*container_list)[i];
          const json::value& c_spec = (*container_specs)[i];
          LOG(INFO) << "Container: " << *c_element;
          const json::Object* container = c_element->As<json::Object>();
          const json::Object* container_spec = c_spec->As<json::Object>();
          const std::string container_name =
              container->Get<json::String>("name");
          const std::string container_id =
              container->Get<json::String>("containerID");
          // TODO: find is_deleted.
          //const json::Object* state = container->Get<json::Object>("state");
          bool is_deleted = false;

          const MonitoredResource gke_container("gke_container", {
            {"cluster_name", cluster_name},
            {"namespace_id", namespace_name},
            {"instance_id", instance_id},
            {"pod_id", pod_id},
            {"container_name", container_name},
            {"zone", zone},
          });

          const std::string gke_container_id = boost::algorithm::join(
              std::vector<std::string>{kGkeContainerResourcePrefix, namespace_name, pod_id, container_name},
              kResourceTypeSeparator);
          const std::string gke_container_name = boost::algorithm::join(
              std::vector<std::string>{kGkeContainerNameResourcePrefix, namespace_name, pod_name, container_name},
              kResourceTypeSeparator);
          result.emplace_back(
              std::vector<std::string>{gke_container_id, gke_container_name},
              gke_container,
              MetadataAgent::Metadata::IGNORED());

          const MonitoredResource k8s_container("k8s_container", {
            {"cluster_name", cluster_name},
            {"namespace_name", namespace_name},
            {"node_name", node_name},
            {"pod_name", pod_name},
            {"container_name", container_name},
            {"location", zone},
          });

          json::value container_raw_metadata = json::object({
            {"blobs", json::object({
              {"association", associations->Clone()},
              {"spec", json::object({
                {"version", json::string(kKubernetesApiVersion)},
                {"raw", container_spec->Clone()},
              })},
              {"status", json::object({
                {"version", json::string(kKubernetesApiVersion)},
                {"raw", container->Clone()},
              })},
              {"labels", json::object({
                {"version", json::string(kKubernetesApiVersion)},
                {"raw", labels->Clone()},
              })},
            })},
          });
          LOG(INFO) << "Raw container metadata: " << *container_raw_metadata;

          const std::string k8s_container_id = boost::algorithm::join(
              std::vector<std::string>{kK8sContainerResourcePrefix, namespace_name, pod_id, container_name},
              kResourceTypeSeparator);
          const std::string k8s_container_name = boost::algorithm::join(
              std::vector<std::string>{kK8sContainerNameResourcePrefix, namespace_name, pod_name, container_name},
              kResourceTypeSeparator);
          result.emplace_back(
              std::vector<std::string>{k8s_container_id, k8s_container_name},
              k8s_container,
#ifdef ENABLE_KUBERNETES_METADATA
              MetadataAgent::Metadata(kKubernetesApiVersion,
                                      is_deleted, created_at, collected_at,
                                      std::move(container_raw_metadata))
#else
              MetadataAgent::Metadata::IGNORED()
#endif
          );
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
  http::client client;
  http::client::request request(endpoint);
  request << boost::network::header(
      "Authorization", "Bearer " + KubernetesApiToken());
  LOG(INFO) << "QueryMaster: Contacting " << endpoint;
  try {
    http::client::response response = client.get(request);
#ifdef VERBOSE
    LOG(DEBUG) << "QueryMaster: Response: " << body(response);
#endif
    return json::Parser::FromString(body(response));
  } catch (const boost::system::system_error& e) {
    LOG(ERROR) << "Failed to query " << endpoint << ": " << e.what();
    throw QueryException(endpoint + " -> " + e.what());
  }
}

const std::string& KubernetesReader::KubernetesApiToken() const {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  if (kubernetes_api_token_.empty()) {
    if (!ReadServiceAccountSecret("token", kubernetes_api_token_)) {
      LOG(ERROR) << "Failed to read Kubernetes API token";
    }
  }
  return kubernetes_api_token_;
}

const std::string& KubernetesReader::KubernetesNamespace() const {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  if (kubernetes_namespace_.empty()) {
    if (!ReadServiceAccountSecret("namespace", kubernetes_api_token_)) {
      LOG(ERROR) << "Failed to read Kubernetes namespace";
    }
  }
  return kubernetes_namespace_;
}

const std::string& KubernetesReader::CurrentNode() const {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  if (current_node_.empty()) {
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
      LOG(INFO) << "Parsed API list: " << *apilist_response;

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
  const auto path_component = KindPath(api_version, kind);
#ifdef VERBOSE
  LOG(DEBUG) << "KindPath returned {" << path_component.first << ", "
             << path_component.second << "}";
#endif
  return QueryMaster(path_component.first + "/namespaces/" + ns + "/" +
                     path_component.second + "/" + name);
}

json::value KubernetesReader::FindTopLevelOwner(
    const std::string& ns, json::value object) const
    throw(QueryException, json::Exception) {
  const json::Object* obj = object->As<json::Object>();
#ifdef VERBOSE
  LOG(DEBUG) << "Looking for the top-level owner for " << *obj;
#endif
  const json::Object* metadata = obj->Get<json::Object>("metadata");
#ifdef VERBOSE
  LOG(DEBUG) << "FindTopLevelOwner: metadata is " << *metadata;
#endif
  if (!metadata->Has("ownerReferences")) {
#ifdef VERBOSE
    LOG(DEBUG) << "FindTopLevelOwner: no owner references in " << *metadata;
#endif
    return object;
  }
  const json::Array* refs = metadata->Get<json::Array>("ownerReferences");
#ifdef VERBOSE
  LOG(DEBUG) << "FindTopLevelOwner: refs is " << *refs;
#endif
  if (refs->size() > 1) {
    LOG(WARNING) << "Found multiple owner references for " << *obj
                 << " picking the first one arbitrarily.";
  }
  const json::value& ref = (*refs)[0];
#ifdef VERBOSE
  LOG(DEBUG) << "FindTopLevelOwner: ref is " << *ref;
#endif
  return FindTopLevelOwner(ns, GetOwner(ns, ref->As<json::Object>()));
}

}
