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

#include "format.h"
#include "http_common.h"
#include "instance.h"
#include "json.h"
#include "logging.h"
#include "resource.h"
#include "time.h"

namespace http = boost::network::http;

namespace google {

namespace {

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

}

KubernetesReader::KubernetesReader(const MetadataAgentConfiguration& config)
    : config_(config), environment_(config) {}

MetadataUpdater::ResourceMetadata KubernetesReader::GetNodeMetadata(
    json::value raw_node, Timestamp collected_at, bool is_deleted) const
    throw(json::Exception) {
  const std::string cluster_name = environment_.KubernetesClusterName();
  const std::string location = environment_.KubernetesClusterLocation();

  const json::Object* node = raw_node->As<json::Object>();
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

  json::value instance_resource =
      InstanceReader::InstanceResource(environment_).ToJSON();

  json::value node_raw_metadata = json::object({
    {"blobs", json::object({
      {"association", json::object({
        {"version", json::string(config_.MetadataIngestionRawContentVersion())},
        {"raw", json::object({
          {"infrastructureResource", std::move(instance_resource)},
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
      std::vector<std::string>{kK8sNodeResourcePrefix, node_name},
      config_.MetadataApiResourceTypeSeparator());
  return MetadataUpdater::ResourceMetadata(
      std::vector<std::string>{k8s_node_name},
      k8s_node,
#ifdef ENABLE_KUBERNETES_METADATA
      MetadataAgent::Metadata(config_.MetadataIngestionRawContentVersion(),
                              is_deleted, created_at, collected_at,
                              std::move(node_raw_metadata))
#else
      MetadataAgent::Metadata::IGNORED()
#endif
  );
}

json::value KubernetesReader::ComputePodAssociations(const json::Object* pod)
    const throw(json::Exception) {
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

  // TODO: What about pods that are not scheduled yet?
  const json::Object* spec = pod->Get<json::Object>("spec");
  const std::string node_name = spec->Get<json::String>("nodeName");

  json::value instance_resource =
      InstanceReader::InstanceResource(environment_).ToJSON();

  return json::object({
    {"version", json::string(config_.MetadataIngestionRawContentVersion())},
    {"raw", json::object({
      {"infrastructureResource", std::move(instance_resource)},
      {"controllers", json::object({
        {"topLevelControllerType", json::string(top_level_kind)},
        {"topLevelControllerName", json::string(top_level_name)},
      })},
      {"nodeName", json::string(node_name)},
    })},
  });
}

MetadataUpdater::ResourceMetadata KubernetesReader::GetPodMetadata(
    json::value raw_pod, json::value associations, Timestamp collected_at,
    bool is_deleted) const throw(json::Exception) {
  const std::string cluster_name = environment_.KubernetesClusterName();
  const std::string location = environment_.KubernetesClusterLocation();

  const json::Object* pod = raw_pod->As<json::Object>();

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
#ifdef ENABLE_KUBERNETES_METADATA
      MetadataAgent::Metadata(config_.MetadataIngestionRawContentVersion(),
                              is_deleted, created_at, collected_at,
                              std::move(pod_raw_metadata))
#else
      MetadataAgent::Metadata::IGNORED()
#endif
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

  std::unique_ptr<json::Object> blobs(new json::Object({
    {"association", std::move(associations)},
    {"spec", json::object({
      {"version", json::string(kKubernetesApiVersion)},
      {"raw", container_spec->Clone()},
    })},
  }));
  if (container_status) {
    blobs->emplace(std::make_pair(
      "status",
      json::object({
        {"version", json::string(kKubernetesApiVersion)},
        {"raw", container_status->Clone()},
      })
    ));
  }
  if (labels) {
    blobs->emplace(std::make_pair(
      "labels",
      json::object({
        {"version", json::string(kKubernetesApiVersion)},
        {"raw", labels->Clone()},
      })
    ));
  }
  json::value container_raw_metadata = json::object({
    {"blobs", json::value(std::move(blobs))},
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
      MetadataAgent::Metadata::IGNORED());
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
    result.emplace_back(GetLegacyResource(pod, name));
    result.emplace_back(
        GetContainerMetadata(pod, container_spec, container_status,
                             associations->Clone(), collected_at, is_deleted));
  }

  result.emplace_back(
      GetPodMetadata(pod->Clone(), std::move(associations), collected_at,
                     is_deleted));
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

    result.emplace_back(GetNodeMetadata(std::move(raw_node), collected_at,
                                        /*is_deleted=*/false));
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
    if (status(response) >= 300) {
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
struct Watcher {
  Watcher(const std::string& endpoint,
          std::function<void(const std::string&)> body_callback,
          std::unique_lock<std::mutex>&& completion, bool verbose)
      : name_("Watcher(" + endpoint + ")"),
        completion_(std::move(completion)), body_callback_(body_callback),
        remaining_chunk_bytes_(0), verbose_(verbose), exception_message_() {}
  ~Watcher() {}  // Unlocks the completion_ lock.

 private:
  // A representation of all watch-related errors.
  class WatcherException {
   public:
    WatcherException(const std::string& what) : explanation_(what) {}
    const std::string& what() const { return explanation_; }
   private:
    std::string explanation_;
  };

 public:
  void operator()(const boost::iterator_range<const char*>& range,
                  const boost::system::error_code& error) {
    if (!error) {
      if (!exception_message_.empty()) {
        // We've encountered an unrecoverable error -- just ignore the rest of
        // the input.
        return;
      }

      try {
//#ifdef VERBOSE
//        LOG(DEBUG) << name_ << " => "
//                   << "Watch notification: '"
//                   << std::string(std::begin(range), std::end(range))
//                   << "'";
//#endif
        boost::iterator_range<const char*> pos = range;
        if (remaining_chunk_bytes_ != 0) {
          pos = ReadNextChunk(pos);
//#ifdef VERBOSE
//          LOG(DEBUG) << name_ << " => "
//                     << "Read another chunk; body now is '" << body_ << "'; "
//                     << remaining_chunk_bytes_ << " bytes remaining";
//#endif
        }

        if (remaining_chunk_bytes_ == 0) {
          // Invoke the callback.
          body_callback_(body_);

          // Process the next batch.
          while (remaining_chunk_bytes_ == 0 &&
                 std::begin(pos) != std::end(pos)) {
            pos = StartNewChunk(pos);
          }
//#ifdef VERBOSE
//          LOG(DEBUG) << name_ << " => "
//                     << "Started new chunk; " << remaining_chunk_bytes_
//                     << " bytes remaining";
//#endif

          if (remaining_chunk_bytes_ != 0) {
            pos = ReadNextChunk(pos);
//#ifdef VERBOSE
//            LOG(DEBUG) << name_ << " => "
//                       << "Read another chunk; body now is '" << body_ << "'; "
//                       << remaining_chunk_bytes_ << " bytes remaining";
//#endif
          }
        }
      } catch (const WatcherException& e) {
        LOG(ERROR) << name_ << " => "
                   << "Callback got exception: " << e.what();
        exception_message_ = e.what();
      }
    } else {
      if (error == boost::asio::error::eof) {
#ifdef VERBOSE
        LOG(DEBUG) << name_ << " => "
                   << "Watch callback: EOF";
#endif
      } else {
        LOG(ERROR) << name_ << " => "
                   << "Callback got error " << error;
      }
      if (verbose_) {
        LOG(ERROR) << name_ << " => "
                   << "Unlocking completion mutex";
      }
      completion_.unlock();
    }
  }

  // The watch callback may throw an exception. Because the watcher runs in a
  // separate thread, the exception will be preserved when the watcher exits,
  // and can be accessed via this function. An empty string is returned when
  // there was no exception.
  const std::string& exception() const { return exception_message_; }

 private:
  boost::iterator_range<const char*>
  StartNewChunk(const boost::iterator_range<const char*>& range)
      throw(WatcherException) {
    if (remaining_chunk_bytes_ != 0) {
      LOG(ERROR) << name_ << " => "
                 << "Starting new chunk with " << remaining_chunk_bytes_
                 << " bytes remaining";
    }

    body_.clear();

    const std::string crlf("\r\n");
    auto begin = std::begin(range);
    auto end = std::end(range);
    auto iter = std::search(begin, end, crlf.begin(), crlf.end());
    if (iter == begin) {
      // Blank lines are fine, just skip them.
      iter = std::next(iter, crlf.size());
#ifdef VERBOSE
      LOG(DEBUG) << name_ << " => "
                 << "Skipping blank line within chunked encoding;"
                 << " remaining data '" << std::string(iter, end) << "'";
#endif
      return boost::iterator_range<const char*>(iter, end);
    } else if (iter == end) {
      throw WatcherException("Invalid chunked encoding: '" +
                             std::string(begin, end) +
                             "'; exiting");
    }
    std::string line(begin, iter);
    iter = std::next(iter, crlf.size());
//#ifdef VERBOSE
//    LOG(DEBUG) << name_ << " => "
//               << "Line: '" << line << "'";
//#endif
    std::istringstream stream(line);
    stream >> std::hex >> remaining_chunk_bytes_;
    return boost::iterator_range<const char*>(iter, end);
  }

  boost::iterator_range<const char*>
  ReadNextChunk(const boost::iterator_range<const char*>& range)
      throw(WatcherException) {
    if (remaining_chunk_bytes_ == 0) {
      throw WatcherException(
          "Asked to read next chunk with no bytes remaining");
    }

    const std::string crlf("\r\n");
    auto begin = std::begin(range);
    auto end = std::end(range);
    // The available bytes in the current notification, which may include the
    // remainder of this chunk and the start of the next one.
    const size_t available = std::distance(begin, end);
    const size_t len = std::min(available, remaining_chunk_bytes_);
    body_.insert(body_.end(), begin, begin + len);
    remaining_chunk_bytes_ -= len;
    begin = std::next(begin, len);
    return boost::iterator_range<const char*>(begin, end);
  }

  std::string name_;
  std::unique_lock<std::mutex> completion_;
  std::function<void(const std::string&)> body_callback_;
  std::string body_;
  size_t remaining_chunk_bytes_;
  bool verbose_;
  std::string exception_message_;
};

void BodyCallback(const std::string& name,
                  std::function<void(json::value)> event_callback,
                  const std::string& body) {
  if (body.empty()) {
#ifdef VERBOSE
    LOG(DEBUG) << name << " => "
               << "Skipping empty watch notification";
#endif
  } else {
    try {
//#ifdef VERBOSE
//      LOG(DEBUG) << name << " => "
//                 << "Invoking callbacks on '" << body << "'";
//#endif
      std::vector<json::value> events = json::Parser::AllFromString(body);
      for (json::value& event : events) {
        std::string event_str = event->ToString();
#ifdef VERBOSE
        LOG(DEBUG) << name << " => "
                   << "Invoking callback('" << event_str << "')";
#endif
        event_callback(std::move(event));
#ifdef VERBOSE
        LOG(DEBUG) << name << " => "
                   << "callback('" << event_str << "') completed";
#endif
      }
//#ifdef VERBOSE
//      LOG(DEBUG) << name << " => "
//                 << "All callbacks on '" << body << "' completed";
//#endif
    } catch (const json::Exception& e) {
      LOG(ERROR) << "Unable to process events: " << e.what();
    }
  }
}

void WatchEventCallback(
    std::function<void(const json::Object*, Timestamp, bool)> callback,
    json::value raw_watch) throw(json::Exception) {
  Timestamp collected_at = std::chrono::system_clock::now();

  //LOG(ERROR) << "Watch callback: " << *raw_watch;
  const json::Object* watch = raw_watch->As<json::Object>();
  const std::string type = watch->Get<json::String>("type");
  const json::Object* object = watch->Get<json::Object>("object");
  LOG(ERROR) << "Watch type: " << type << " object: " << *object;
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
  http::client client(
      http::client::options()
      .remove_chunk_markers(false)
      .openssl_certificate(SecretPath("ca.crt")));
  http::client::request request(endpoint);
  request << boost::network::header(
      "Authorization", "Bearer " + KubernetesApiToken());
  if (config_.VerboseLogging()) {
    LOG(INFO) << "WatchMaster(" << name << "): Contacting " << endpoint;
  }
  try {
    if (config_.VerboseLogging()) {
      LOG(INFO) << "Locking completion mutex";
    }
    // A notification for watch completion.
    std::mutex completion_mutex;
    std::unique_lock<std::mutex> watch_completion(completion_mutex);
    Watcher watcher(
      endpoint,
      [=](const std::string& body) {
        BodyCallback(name,
                     [=](json::value raw_watch) {
                       WatchEventCallback(callback, std::move(raw_watch));
                     },
                     body);
      },
      std::move(watch_completion), config_.VerboseLogging());
    http::client::response response = client.get(request, std::ref(watcher));
    if (config_.VerboseLogging()) {
      LOG(INFO) << "Waiting for completion";
    }
    std::lock_guard<std::mutex> await_completion(completion_mutex);
    if (!watcher.exception().empty()) {
      LOG(ERROR) << "Exception from the watcher thread: "
                 << watcher.exception();
      throw QueryException(watcher.exception());
    }
    if (config_.VerboseLogging()) {
      LOG(INFO) << "WatchMaster(" << name << ") completed " << body(response);
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

bool KubernetesReader::ValidateConfiguration() const {
  try {
    (void) QueryMaster(std::string(kKubernetesEndpointPath) + "/nodes?limit=1");
  } catch (const QueryException& e) {
    // Already logged.
    return false;
  }

  try {
    const std::string pod_label_selector(
        config_.KubernetesPodLabelSelector().empty()
        ? "" : "&" + config_.KubernetesPodLabelSelector());

    (void) QueryMaster(std::string(kKubernetesEndpointPath) + "/pods?limit=1" +
                       pod_label_selector);
  } catch (const QueryException& e) {
    // Already logged.
    return false;
  }

  if (CurrentNode().empty()) {
    return false;
  }

  return true;
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
  LOG(INFO) << "Watch thread (pods) exiting";
}

void KubernetesReader::NodeCallback(
    MetadataUpdater::UpdateCallback callback,
    const json::Object* node, Timestamp collected_at, bool is_deleted) const
    throw(json::Exception) {
  std::vector<MetadataUpdater::ResourceMetadata> result_vector;
  result_vector.emplace_back(
      GetNodeMetadata(node->Clone(), collected_at, is_deleted));
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
  LOG(INFO) << "Watch thread (node) exiting";
}

KubernetesUpdater::KubernetesUpdater(MetadataAgent* server)
    : reader_(server->config()), PollingMetadataUpdater(
        server, "KubernetesUpdater",
        server->config().KubernetesUpdaterIntervalSeconds(),
        [=]() { return reader_.MetadataQuery(); }) { }

bool KubernetesUpdater::ValidateConfiguration() const {
  if (!PollingMetadataUpdater::ValidateConfiguration()) {
    return false;
  }

  return reader_.ValidateConfiguration();
}

void KubernetesUpdater::StartUpdater() {
  if (config().KubernetesUseWatch()) {
    const std::string current_node = reader_.CurrentNode();

    if (config().VerboseLogging()) {
      LOG(INFO) << "Current node is " << current_node;
    }

    auto cb = [=](std::vector<MetadataUpdater::ResourceMetadata>&& results) {
      MetadataCallback(std::move(results));
    };
    node_watch_thread_ = std::thread([=]() {
      reader_.WatchNodes(current_node, cb);
    });
    pod_watch_thread_ = std::thread([=]() {
      reader_.WatchPods(current_node, cb);
    });
  } else {
    // Only try to poll if watch is disabled.
    PollingMetadataUpdater::StartUpdater();
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
