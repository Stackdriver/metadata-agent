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
#include <tuple>

#include "http_common.h"
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

constexpr const char kWatchParam[] = "watch=true";

constexpr const char kDockerIdPrefix[] = "docker://";

constexpr const char kServiceAccountDirectory[] =
    "/var/run/secrets/kubernetes.io/serviceaccount";

// Reads a Kubernetes service account secret file into the provided string.
// Returns true if the file was read successfully.
bool ReadServiceAccountSecret(
    const std::string& secret, std::string& destination, bool verbose) {
  std::string filename(std::string(kServiceAccountDirectory) + "/" + secret);
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

}

KubernetesReader::KubernetesReader(const MetadataAgentConfiguration& config)
    : config_(config), environment_(config) {}

MetadataUpdater::ResourceMetadata KubernetesReader::GetNodeMetadata(
    json::value raw_node, Timestamp collected_at) const throw(json::Exception) {
  const std::string platform = "gce";  // TODO: detect other platforms.
  const std::string instance_id = environment_.InstanceId();
  const std::string zone = environment_.InstanceZone();
  const std::string cluster_name = environment_.KubernetesClusterName();

  const json::Object* node = raw_node->As<json::Object>();
  const json::Object* metadata = node->Get<json::Object>("metadata");
  const std::string node_name = metadata->Get<json::String>("name");
  const std::string created_str =
      metadata->Get<json::String>("creationTimestamp");
  Timestamp created_at = rfc3339::FromString(created_str);

  const MonitoredResource k8s_node("k8s_node", {
    {"cluster_name", cluster_name},
    {"node_name", node_name},
    {"location", zone},
  });

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
  if (config_.VerboseLogging()) {
    LOG(INFO) << "Raw node metadata: " << *node_raw_metadata;
  }

  const std::string k8s_node_name = boost::algorithm::join(
      std::vector<std::string>{kK8sNodeNameResourcePrefix, node_name},
      kResourceTypeSeparator);
  return MetadataUpdater::ResourceMetadata(
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
}

json::value KubernetesReader::ComputePodAssociations(const json::Object* pod)
    const throw(json::Exception) {
  const std::string platform = "gce";  // TODO: detect other platforms.

  const json::Object* metadata = pod->Get<json::Object>("metadata");
  const std::string namespace_name = metadata->Get<json::String>("namespace");
  const std::string pod_id = metadata->Get<json::String>("uid");

  const json::value top_level = FindTopLevelOwner(namespace_name, pod->Clone());
  const json::Object* top_level_controller = top_level->As<json::Object>();
  const json::Object* top_level_metadata =
      top_level_controller->Get<json::Object>("metadata");
  const std::string top_level_name =
      top_level_metadata->Get<json::String>("name");
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

  return json::object({
    {"version", json::string(kRawContentVersion)},
    {"raw", json::object({
      {"providerPlatform", json::string(platform)},
      {"controllers", json::object({
        {"topLevelControllerType", json::string(top_level_kind)},
        {"topLevelControllerName", json::string(top_level_name)},
      })},
    })},
  });
}

MetadataUpdater::ResourceMetadata KubernetesReader::GetPodMetadata(
    json::value raw_pod, json::value associations, Timestamp collected_at) const
    throw(json::Exception) {
  const std::string zone = environment_.InstanceZone();
  const std::string cluster_name = environment_.KubernetesClusterName();

  const json::Object* pod = raw_pod->As<json::Object>();

  const json::Object* metadata = pod->Get<json::Object>("metadata");
  const std::string namespace_name = metadata->Get<json::String>("namespace");
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
  const std::string node_name = spec->Get<json::String>("nodeName");

  const MonitoredResource k8s_pod("k8s_pod", {
    {"cluster_name", cluster_name},
    {"namespace_name", namespace_name},
    {"node_name", node_name},
    {"pod_name", pod_name},
    {"location", zone},
  });

  // TODO: find is_deleted.
  //const json::Object* status = pod->Get<json::Object>("status");
  bool is_deleted = false;

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
      std::vector<std::string>{kK8sPodResourcePrefix, namespace_name, pod_id},
      kResourceTypeSeparator);
  const std::string k8s_pod_name = boost::algorithm::join(
      std::vector<std::string>{kK8sPodNameResourcePrefix, namespace_name, pod_name},
      kResourceTypeSeparator);
  return MetadataUpdater::ResourceMetadata(
      std::vector<std::string>{k8s_pod_id, k8s_pod_name},
      k8s_pod,
#ifdef ENABLE_KUBERNETES_METADATA
      MetadataAgent::Metadata(kRawContentVersion,
                              is_deleted, created_at, collected_at,
                              std::move(pod_raw_metadata))
#else
      MetadataAgent::Metadata::IGNORED()
#endif
  );
}

MetadataUpdater::ResourceMetadata KubernetesReader::GetContainerMetadata(
    const json::Object* pod, int container_index, json::value associations,
    Timestamp collected_at) const throw(json::Exception) {
  const std::string zone = environment_.InstanceZone();
  const std::string cluster_name = environment_.KubernetesClusterName();

  const json::Object* metadata = pod->Get<json::Object>("metadata");
  const std::string namespace_name = metadata->Get<json::String>("namespace");
  const std::string pod_name = metadata->Get<json::String>("name");
  const std::string pod_id = metadata->Get<json::String>("uid");
  const std::string created_str =
      metadata->Get<json::String>("creationTimestamp");
  Timestamp created_at = rfc3339::FromString(created_str);
  const json::Object* labels = metadata->Get<json::Object>("labels");

  const json::Object* spec = pod->Get<json::Object>("spec");
  const std::string node_name = spec->Get<json::String>("nodeName");

  const json::Object* status = pod->Get<json::Object>("status");

  const json::Array* container_specs = spec->Get<json::Array>("containers");
  const json::Array* container_statuses =
      status->Get<json::Array>("containerStatuses");

  const json::value& c_status = (*container_statuses)[container_index];
  const json::value& c_spec = (*container_specs)[container_index];
  if (config_.VerboseLogging()) {
    LOG(INFO) << "Container: " << *c_status;
  }
  const json::Object* container = c_status->As<json::Object>();
  const json::Object* container_spec = c_spec->As<json::Object>();
  const std::string container_name = container->Get<json::String>("name");
  std::size_t docker_prefix_end = sizeof(kDockerIdPrefix) - 1;
  if (container->Get<json::String>("containerID").compare(
          0, docker_prefix_end, kDockerIdPrefix) != 0) {
    LOG(ERROR) << "ContainerID "
               << container->Get<json::String>("containerID")
               << " does not start with " << kDockerIdPrefix
               << " (" << docker_prefix_end << " chars)";
    docker_prefix_end = 0;
  }
  const std::string container_id =
      container->Get<json::String>("containerID").substr(
          docker_prefix_end);
  // TODO: find is_deleted.
  //const json::Object* state = container->Get<json::Object>("state");
  bool is_deleted = false;

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
      {"association", std::move(associations)},
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
  if (config_.VerboseLogging()) {
    LOG(INFO) << "Raw container metadata: " << *container_raw_metadata;
  }

  const std::string k8s_container_id = boost::algorithm::join(
      std::vector<std::string>{kK8sContainerResourcePrefix, container_id},
      kResourceTypeSeparator);
  const std::string k8s_container_pod = boost::algorithm::join(
      std::vector<std::string>{kK8sContainerResourcePrefix, pod_id, container_name},
      kResourceTypeSeparator);
  const std::string k8s_container_name = boost::algorithm::join(
      std::vector<std::string>{kK8sContainerNameResourcePrefix, namespace_name, pod_name, container_name},
      kResourceTypeSeparator);
  return MetadataUpdater::ResourceMetadata(
      std::vector<std::string>{k8s_container_id, k8s_container_pod, k8s_container_name},
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

MetadataUpdater::ResourceMetadata KubernetesReader::GetLegacyResource(
    const json::Object* pod, int container_index) const
    throw(json::Exception) {
  const std::string instance_id = environment_.InstanceId();
  const std::string zone = environment_.InstanceZone();
  const std::string cluster_name = environment_.KubernetesClusterName();

  const json::Object* metadata = pod->Get<json::Object>("metadata");
  const std::string namespace_name = metadata->Get<json::String>("namespace");
  const std::string pod_name = metadata->Get<json::String>("name");
  const std::string pod_id = metadata->Get<json::String>("uid");

  const json::Object* status = pod->Get<json::Object>("status");

  const json::Array* container_statuses =
      status->Get<json::Array>("containerStatuses");

  const json::value& c_status = (*container_statuses)[container_index];
  if (config_.VerboseLogging()) {
    LOG(INFO) << "Container: " << *c_status;
  }
  const json::Object* container = c_status->As<json::Object>();
  const std::string container_name = container->Get<json::String>("name");

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
      kResourceTypeSeparator);
  const std::string gke_container_name = boost::algorithm::join(
      std::vector<std::string>{kGkeContainerNameResourcePrefix, namespace_name, pod_name, container_name},
      kResourceTypeSeparator);
  return MetadataUpdater::ResourceMetadata(
      std::vector<std::string>{gke_container_pod_id, gke_container_name},
      gke_container,
      MetadataAgent::Metadata::IGNORED());
}

std::vector<MetadataUpdater::ResourceMetadata>
KubernetesReader::GetPodAndContainerMetadata(
    const json::Object* pod, Timestamp collected_at) const
    throw(json::Exception) {
  std::vector<MetadataUpdater::ResourceMetadata> result;

  json::value associations = ComputePodAssociations(pod);

  const json::Object* metadata = pod->Get<json::Object>("metadata");
  const std::string pod_name = metadata->Get<json::String>("name");
  const std::string pod_id = metadata->Get<json::String>("uid");

  const json::Object* spec = pod->Get<json::Object>("spec");
  const std::string node_name = spec->Get<json::String>("nodeName");

  const json::Object* status = pod->Get<json::Object>("status");

  const json::Array* container_specs = spec->Get<json::Array>("containers");
  const json::Array* container_statuses =
      status->Get<json::Array>("containerStatuses");
  std::size_t num_containers = std::min(
      container_statuses->size(), container_specs->size());

  for (int i = 0; i < num_containers; ++i) {
    const json::value& c_status = (*container_statuses)[i];
    const json::value& c_spec = (*container_specs)[i];
    if (config_.VerboseLogging()) {
      LOG(INFO) << "Container: " << *c_status;
    }
    const json::Object* container = c_status->As<json::Object>();
    const json::Object* container_spec = c_spec->As<json::Object>();
    const std::string container_name = container->Get<json::String>("name");
    const std::string spec_name = container_spec->Get<json::String>("name");
    if (container_name != spec_name) {
      LOG(ERROR) << "Internal error; container name " << container_name
                 << " not the same as spec name " << spec_name
                 << " at index " << i;
    }
    result.emplace_back(GetLegacyResource(pod, i));
    result.emplace_back(
        GetContainerMetadata(pod, i, associations->Clone(), collected_at));
  }

  result.emplace_back(
      GetPodMetadata(pod->Clone(), std::move(associations), collected_at));
  return std::move(result);
}

std::vector<MetadataUpdater::ResourceMetadata>
    KubernetesReader::MetadataQuery() const {
  if (config_.VerboseLogging()) {
    LOG(INFO) << "Kubernetes Query called";
  }
  std::vector<MetadataUpdater::ResourceMetadata> result;

  const std::string node_name = CurrentNode();

  if (config_.VerboseLogging()) {
    LOG(INFO) << "Current node is " << node_name;
  }

  try {
    json::value raw_node = QueryMaster(
        std::string(kKubernetesEndpointPath) + "/nodes/" + node_name);
    Timestamp collected_at = std::chrono::system_clock::now();

    result.emplace_back(GetNodeMetadata(std::move(raw_node), collected_at));
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
        const std::string pod_node_name = spec->Get<json::String>("nodeName");
        if (pod_node_name != node_name) {
          LOG(ERROR) << "Internal error; pod's node " << pod_node_name
                     << " not the same as agent node " << node_name;
        }

        const json::Object* status = pod->Get<json::Object>("status");

        const json::Array* container_specs = spec->Get<json::Array>("containers");
        const json::Array* container_statuses =
            status->Get<json::Array>("containerStatuses");
        if (container_specs->size() != container_statuses->size()) {
          LOG(ERROR) << "Container specs and statuses arrays "
                     << "have different sizes: "
                     << container_specs->size() << " vs "
                     << container_statuses->size() << " for pod "
                     << pod_id << "(" << pod_name << ")";
        }

        std::vector<MetadataUpdater::ResourceMetadata> full_pod_metadata =
            GetPodAndContainerMetadata(pod, collected_at);
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
  http::client client;
  http::client::request request(endpoint);
  request << boost::network::header(
      "Authorization", "Bearer " + KubernetesApiToken());
  if (config_.VerboseLogging()) {
    LOG(INFO) << "QueryMaster: Contacting " << endpoint;
  }
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

namespace {
struct Watcher {
  Watcher(std::function<void(json::value)> callback,
          std::unique_lock<std::mutex>&& completion, bool verbose)
      : completion_(std::move(completion)), callback_(callback),
        remaining_bytes_(0), verbose_(verbose) {}
  ~Watcher() {}  // Unlocks the completion_ lock.
  void operator()(const boost::iterator_range<const char*>& range,
                  const boost::system::error_code& error) {
    if (!error) {
//#ifdef VERBOSE
//      LOG(DEBUG) << "Watch notification: '"
//                 << std::string(std::begin(range), std::end(range))
//                 << "'";
//#endif
      boost::iterator_range<const char*> pos = range;
      if (remaining_bytes_ != 0) {
        pos = ReadNextChunk(pos);
//#ifdef VERBOSE
//        LOG(DEBUG) << "Read another chunk; body now is '" << body_ << "'; "
//                   << remaining_bytes_ << " bytes remaining";
//#endif
      }

      if (remaining_bytes_ == 0) {
        // Invoke the callback.
        CompleteChunk();

        // Process the next batch.
        while (remaining_bytes_ == 0 && std::begin(pos) != std::end(pos)) {
          pos = StartNewChunk(pos);
        }
//#ifdef VERBOSE
//        LOG(DEBUG) << "Started new chunk; " << remaining_bytes_
//                   << " bytes remaining";
//#endif

        if (remaining_bytes_ != 0) {
          pos = ReadNextChunk(pos);
//#ifdef VERBOSE
//          LOG(DEBUG) << "Read another chunk; body now is '" << body_ << "'; "
//                     << remaining_bytes_ << " bytes remaining";
//#endif
        }
      }
    } else {
      if (error == boost::asio::error::eof) {
#ifdef VERBOSE
        LOG(DEBUG) << "Watch callback: EOF";
#endif
      } else {
        LOG(ERROR) << "Callback got error " << error;
      }
      if (verbose_) {
        LOG(INFO) << "Unlocking completion mutex";
      }
      completion_.unlock();
    }
  }

 private:
  boost::iterator_range<const char*>
  StartNewChunk(const boost::iterator_range<const char*>& range) {
    if (remaining_bytes_ != 0) {
      LOG(ERROR) << "Starting new chunk with " << remaining_bytes_
                 << " bytes remaining";
    }

    body_.clear();

    const std::string crlf("\r\n");
    auto begin = std::begin(range);
    auto end = std::end(range);
    auto iter = std::search(begin, end, crlf.begin(), crlf.end());
    if (iter == end && iter != begin) {
      LOG(ERROR) << "Invalid chunked encoding: '"
                 << std::string(begin, end)
                 << "'";
      return boost::iterator_range<const char*>(begin, end);
    }
    std::string line(begin, iter);
    iter = std::next(iter, crlf.size());
    if (line.empty()) {
      // Blank lines are fine, just skip them.
#ifdef VERBOSE
      LOG(DEBUG) << "Skipping blank line within chunked encoding;"
                 << " remaining data '" << std::string(iter, end) << "'";
#endif
    } else {
//#ifdef VERBOSE
//      LOG(DEBUG) << "Line: '" << line << "'";
//#endif
      std::stringstream stream(line);
      stream >> std::hex >> remaining_bytes_;
    }
    return boost::iterator_range<const char*>(iter, end);
  }

  boost::iterator_range<const char*>
  ReadNextChunk(const boost::iterator_range<const char*>& range) {
    if (remaining_bytes_ == 0) {
      LOG(ERROR) << "Asked to read next chunk with no bytes remaining";
      return range;
    }

    const std::string crlf("\r\n");
    auto begin = std::begin(range);
    auto end = std::end(range);
    const size_t available = std::distance(begin, end);
    const size_t len = std::min(available, remaining_bytes_);
    body_.insert(body_.end(), begin, begin + len);
    remaining_bytes_ -= len;
    begin = std::next(begin, len);
    return boost::iterator_range<const char*>(begin, end);
  }

  void CompleteChunk() {
    if (body_.empty()) {
#ifdef VERBOSE
      LOG(DEBUG) << "Skipping empty watch notification";
#endif
    } else {
      try {
//#ifdef VERBOSE
//        LOG(DEBUG) << "Invoking callbacks on '" << body_ << "'";
//#endif
        std::vector<json::value> events = json::Parser::AllFromString(body_);
        for (json::value& event : events) {
          std::string event_str = event->ToString();
#ifdef VERBOSE
          LOG(DEBUG) << "Invoking callback('" << event_str << "')";
#endif
          callback_(std::move(event));
#ifdef VERBOSE
          LOG(DEBUG) << "callback('" << event_str << "') completed";
#endif
        }
//#ifdef VERBOSE
//        LOG(DEBUG) << "All callbacks on '" << body_ << "' completed";
//#endif
      } catch (const json::Exception& e) {
        LOG(ERROR) << e.what();
      }
    }
  }

  std::unique_lock<std::mutex> completion_;
  std::function<void(json::value)> callback_;
  std::string body_;
  size_t remaining_bytes_;
  bool verbose_;
};
}

void KubernetesReader::WatchMaster(
    const std::string& path, std::function<void(json::value)> callback) const
    throw(QueryException, json::Exception) {
  const std::string prefix((path.find('?') == std::string::npos) ? "?" : "&");
  const std::string watch_param(prefix + kWatchParam);
  const std::string endpoint(
      config_.KubernetesEndpointHost() + path + watch_param);
  http::client client;
  http::client::request request(endpoint);
  request << boost::network::header(
      "Authorization", "Bearer " + KubernetesApiToken());
  if (config_.VerboseLogging()) {
    LOG(INFO) << "WatchMaster: Contacting " << endpoint;
  }
  try {
    if (config_.VerboseLogging()) {
      LOG(INFO) << "Locking completion mutex";
    }
    // A notification for watch completion.
    std::mutex completion_mutex;
    std::unique_lock<std::mutex> watch_completion(completion_mutex);
    Watcher watcher(callback, std::move(watch_completion),
                    config_.VerboseLogging());
    http::client::response response = client.get(request, boost::ref(watcher));
    if (config_.VerboseLogging()) {
      LOG(INFO) << "Waiting for completion";
    }
    std::lock_guard<std::mutex> await_completion(completion_mutex);
    if (config_.VerboseLogging()) {
      LOG(INFO) << "WatchMaster completed " << body(response);
    }
    std::string encoding;
#ifdef VERBOSE
    LOG(DEBUG) << "response headers: " << response.headers();
#endif
    auto transfer_encoding_header = headers(response)["Transfer-Encoding"];
    if (!boost::empty(transfer_encoding_header)) {
      encoding = boost::begin(transfer_encoding_header)->second;
    }
    if (encoding != "chunked") {
      LOG(ERROR) << "Expected chunked encoding; found '" << encoding << "'";
    }
  } catch (const boost::system::system_error& e) {
    LOG(ERROR) << "Failed to query " << endpoint << ": " << e.what();
    throw QueryException(endpoint + " -> " + e.what());
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
  auto found = owners_.emplace(std::piecewise_construct,
                               std::forward_as_tuple(encoded_ref),
                               std::forward_as_tuple());
  json::value& owner = found.first->second;
  if (found.second) {  // Not found, inserted new.
    const auto path_component = KindPath(api_version, kind);
#ifdef VERBOSE
    LOG(DEBUG) << "KindPath returned {" << path_component.first << ", "
               << path_component.second << "}";
#endif
    owner = std::move(
        QueryMaster(path_component.first + "/namespaces/" + ns + "/" +
                    path_component.second + "/" + name));
    // Sanity check: because we are looking up by name, the object we get
    // back might have a different uid.
    const json::Object* owner_obj = owner->As<json::Object>();
    const json::Object* metadata = owner_obj->Get<json::Object>("metadata");
    const std::string owner_uid = metadata->Get<json::String>("uid");
    if (owner_uid != uid) {
      LOG(WARNING) << "Owner " << kind << "'" << name << "' (id " << uid
                   << ") disappeared before we could query it. Found id "
                   << owner_uid << " instead.";
      owner.reset();
      throw QueryException("Owner " + kind + " " + name + " (id " + uid +
                           ") disappeared");
    }
  }
  return owner->Clone();
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

void KubernetesReader::PodCallback(MetadataUpdater::UpdateCallback callback,
                                   json::value raw_watch) const
    throw(json::Exception) {
  Timestamp collected_at = std::chrono::system_clock::now();

  //LOG(ERROR) << "Watch callback: " << *raw_watch;
  const json::Object* watch = raw_watch->As<json::Object>();
  const std::string type = watch->Get<json::String>("type");
  const json::Object* pod = watch->Get<json::Object>("object");
  LOG(ERROR) << "Watch type: " << type << " object: " << *pod;
  if (type == "MODIFIED" || type == "ADDED") {
    std::vector<MetadataUpdater::ResourceMetadata> result_vector =
        GetPodAndContainerMetadata(pod, collected_at);
    callback(std::move(result_vector));
  }
}

void KubernetesReader::WatchPods(MetadataUpdater::UpdateCallback callback)
    const {
  LOG(INFO) << "Watch thread (pods) started";

  const std::string node_name = CurrentNode();

  if (config_.VerboseLogging()) {
    LOG(INFO) << "Current node is " << node_name;
  }

  const std::string node_selector(kNodeSelectorPrefix + node_name);
  const std::string pod_label_selector(
      config_.KubernetesPodLabelSelector().empty()
      ? "" : "&" + config_.KubernetesPodLabelSelector());

  try {
    WatchMaster(std::string(kKubernetesEndpointPath) + "/pods"
                + node_selector + pod_label_selector,
                std::bind(&KubernetesReader::PodCallback,
                          this, callback, std::placeholders::_1));
  } catch (const json::Exception& e) {
    LOG(ERROR) << e.what();
  } catch (const KubernetesReader::QueryException& e) {
    // Already logged.
  }
  LOG(INFO) << "Watch thread (pods) exiting";
}

void KubernetesReader::NodeCallback(MetadataUpdater::UpdateCallback callback,
                                    json::value raw_watch) const
    throw(json::Exception) {
  Timestamp collected_at = std::chrono::system_clock::now();

  //LOG(ERROR) << "Watch callback: " << *raw_watch;
  const json::Object* watch = raw_watch->As<json::Object>();
  const std::string type = watch->Get<json::String>("type");
  const json::Object* node = watch->Get<json::Object>("object");
  LOG(ERROR) << "Watch type: " << type << " object: " << *node;
  if (type == "MODIFIED" || type == "ADDED") {
    std::vector<MetadataUpdater::ResourceMetadata> result_vector;
    result_vector.emplace_back(GetNodeMetadata(node->Clone(), collected_at));
    callback(std::move(result_vector));
  }
}

void KubernetesReader::WatchNode(MetadataUpdater::UpdateCallback callback)
    const {
  LOG(INFO) << "Watch thread (node) started";

  const std::string node_name = CurrentNode();

  if (config_.VerboseLogging()) {
    LOG(INFO) << "Current node is " << node_name;
  }

  try {
    // TODO: There seems to be a Kubernetes API bug with watch=true.
    WatchMaster(std::string(kKubernetesEndpointPath) + "/watch/nodes/"
                + node_name,
                std::bind(&KubernetesReader::NodeCallback,
                          this, callback, std::placeholders::_1));
  } catch (const json::Exception& e) {
    LOG(ERROR) << e.what();
  } catch (const KubernetesReader::QueryException& e) {
    // Already logged.
  }
  LOG(INFO) << "Watch thread (node) exiting";
}

void KubernetesUpdater::MetadataCallback(
    std::vector<MetadataUpdater::ResourceMetadata>&& result_vector) {
  for (MetadataUpdater::ResourceMetadata& result : result_vector) {
    UpdateResourceCallback(result);
    UpdateMetadataCallback(std::move(result));
  }
}

}
