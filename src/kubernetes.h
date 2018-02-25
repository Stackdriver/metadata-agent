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
#ifndef KUBERNETES_H_
#define KUBERNETES_H_

//#include "config.h"

#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include "configuration.h"
#include "environment.h"
#include "json.h"
#include "updater.h"

namespace google {

class KubernetesReader {
 public:
  KubernetesReader(const MetadataAgentConfiguration& config);
  // A Kubernetes metadata query function.
  std::vector<MetadataUpdater::ResourceMetadata> MetadataQuery() const;

  // Node watcher.
  void WatchNode(MetadataUpdater::UpdateCallback callback) const;

  // Pod watcher.
  void WatchPods(MetadataUpdater::UpdateCallback callback) const;

 private:
  // A representation of all query-related errors.
  class QueryException {
   public:
    QueryException(const std::string& what) : explanation_(what) {}
    const std::string& what() const { return explanation_; }
   private:
    std::string explanation_;
  };

  // Issues a Kubernetes master API query at a given path and
  // returns a parsed JSON response. The path has to start with "/".
  json::value QueryMaster(const std::string& path) const
      throw(QueryException, json::Exception);

  // Issues a Kubernetes master API query at a given path and
  // watches for parsed JSON responses. The path has to start with "/".
  // Invokes callback for every notification.
  void WatchMaster(
    const std::string& path, std::function<void(json::value)> callback) const
    throw(QueryException, json::Exception);

  // Node watcher callback.
  void NodeCallback(
      MetadataUpdater::UpdateCallback callback, json::value result) const
      throw(json::Exception);

  // Pod watcher callback.
  void PodCallback(
      MetadataUpdater::UpdateCallback callback, json::value result) const
      throw(json::Exception);

  // Compute the associations for a given pod.
  json::value ComputePodAssociations(const json::Object* pod) const
      throw(json::Exception);
  // Given a node object, return the associated metadata.
  MetadataUpdater::ResourceMetadata GetNodeMetadata(
      json::value raw_node, Timestamp collected_at, bool is_deleted) const
      throw(json::Exception);
  // Given a pod object, return the associated metadata.
  MetadataUpdater::ResourceMetadata GetPodMetadata(
      json::value raw_pod, json::value associations, Timestamp collected_at,
      bool is_deleted) const throw(json::Exception);
  // Given a pod object and container info, return the container metadata.
  MetadataUpdater::ResourceMetadata GetContainerMetadata(
      const json::Object* pod, const json::Object* container_spec,
      const json::Object* container_status, json::value associations,
      Timestamp collected_at, bool is_deleted) const throw(json::Exception);
  // Given a pod object and container name, return the legacy resource.
  // The returned "metadata" field will be Metadata::IGNORED.
  MetadataUpdater::ResourceMetadata GetLegacyResource(
      const json::Object* pod, const std::string& container_name) const
      throw(json::Exception);
  // Given a pod object, return the associated pod and container metadata.
  std::vector<MetadataUpdater::ResourceMetadata> GetPodAndContainerMetadata(
      const json::Object* pod, Timestamp collected_at, bool is_deleted) const
      throw(json::Exception);

  // Gets the name of the node the agent is running on.
  // Returns an empty string if unable to find the current node.
  const std::string& CurrentNode() const;

  // Gets the Kubernetes master API token.
  // Returns an empty string if unable to find the token.
  const std::string& KubernetesApiToken() const;

  // Gets the Kubernetes namespace for the current container.
  // Returns an empty string if unable to find the namespace.
  const std::string& KubernetesNamespace() const;

  // Given a version string, returns the path and name for a given kind.
  std::pair<std::string, std::string> KindPath(const std::string& version,
                                               const std::string& kind) const;

  // Follows the owner reference to get the corresponding object.
  json::value GetOwner(const std::string& ns, const json::Object* owner_ref)
      const throw(QueryException, json::Exception);

  // For a given object, returns the top-level owner object.
  // When there are multiple owner references, follows the first one.
  json::value FindTopLevelOwner(const std::string& ns, json::value object) const
      throw(QueryException, json::Exception);

  // Cached data.
  mutable std::recursive_mutex mutex_;
  mutable std::string current_node_;
  mutable std::string kubernetes_api_token_;
  mutable std::string kubernetes_namespace_;
  // A memoized map from version to a map from kind to name.
  mutable std::map<std::string, std::map<std::string, std::string>>
      version_to_kind_to_name_;
  // A memoized map from an encoded owner reference to the owner object.
  mutable std::map<std::string, json::value> owners_;

  const MetadataAgentConfiguration& config_;
  Environment environment_;
};

class KubernetesUpdater : public PollingMetadataUpdater {
 public:
  KubernetesUpdater(MetadataAgent* server)
      : reader_(server->config()), PollingMetadataUpdater(
          server, server->config().KubernetesUpdaterIntervalSeconds(),
          std::bind(&google::KubernetesReader::MetadataQuery, &reader_)) { }
  ~KubernetesUpdater() {
    if (node_watch_thread_.joinable()) {
      node_watch_thread_.join();
    }
    if (pod_watch_thread_.joinable()) {
      pod_watch_thread_.join();
    }
  }

  void start();

 private:
  // Metadata watcher callback.
  void MetadataCallback(std::vector<ResourceMetadata>&& result_vector);

  KubernetesReader reader_;
  std::thread node_watch_thread_;
  std::thread pod_watch_thread_;
};

}

#endif  // KUBERNETES_H_
