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

#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include "environment.h"
#include "health_checker.h"
#include "json.h"
#include "resource.h"
#include "updater.h"

namespace google {

// Configuration object.
class Configuration;

// Storage for the metadata mapping.
class MetadataStore;

class KubernetesReader {
 public:
  KubernetesReader(const Configuration& config,
                   HealthChecker* health_checker);
  // A Kubernetes metadata query function.
  std::vector<MetadataUpdater::ResourceMetadata> MetadataQuery() const;

  // Validates the relevant dynamic configuration and throws if it's incorrect.
  void ValidateDynamicConfiguration() const
      throw(MetadataUpdater::ConfigurationValidationError);

  // Node watcher.
  void WatchNodes(const std::string& node_name,
                  MetadataUpdater::UpdateCallback update_cb) const;

  // Pod watcher.
  void WatchPods(const std::string& node_name,
                 MetadataUpdater::UpdateCallback update_cb) const;

  // Generic Kubernetes object watcher.
  void WatchObjects(
      const std::string& plural_kind, const std::string& version,
      MetadataUpdater::UpdateCallback update_cb) const;

  // Gets the name of the node the agent is running on.
  // Returns an empty string if unable to find the current node.
  const std::string& CurrentNode() const;

 private:
  friend class KubernetesTest;
  // A representation of all query-related errors.
  class QueryException {
   public:
    QueryException(const std::string& what) : explanation_(what) {}
    const std::string& what() const { return explanation_; }
   private:
    std::string explanation_;
  };
  class NonRetriableError;

  // Build the watch endpoint given the Kubernetes resource kind and API
  // version.
  const std::string GetWatchEndpoint(
      const std::string& plural_kind, const std::string& api_version,
      const std::string& selector) const;

  // Issues a Kubernetes master API query at a given path and
  // returns a parsed JSON response. The path has to start with "/".
  json::value QueryMaster(const std::string& path) const
      throw(QueryException, json::Exception);

  // Issues a Kubernetes master API query at a given endpoint and
  // watches for parsed JSON responses.
  // Invokes callback for every notification.
  // The name is for logging purposes.
  void WatchMaster(
    const std::string& name,
    const std::string& endpoint,
    std::function<void(const json::Object*, Timestamp, bool)> callback) const
    throw(QueryException, json::Exception);

  using IdsAndMR = std::pair<std::vector<std::string>, MonitoredResource>;
  using MetadataCallback = std::function<void(const json::Object*, IdsAndMR&)>;

  // Kubernetes resource watch callback.
  void ObjectWatchCallback(
      MetadataCallback metadata_cb,
      MetadataUpdater::UpdateCallback update_cb,
      const json::Object* object, Timestamp collected_at, bool is_deleted) const
      throw(json::Exception);

  // Watches the specified endpoint.
  void WatchEndpoint(
      const std::string& plural_kind, const std::string& endpoint,
      MetadataCallback metadata_cb,
      MetadataUpdater::UpdateCallback update_cb) const;

  // Builds the cluster full name from cluster related environment variables.
  const std::string ClusterFullName() const;

  // Computes the full resource name given the self link.
  const std::string FullResourceName(const std::string& self_link) const;

  // Given a generic Kubernetes object, return the associated metadata, with the
  // local ID and MonitoredResource set to blank values.
  MetadataUpdater::ResourceMetadata GetObjectMetadata(
      const json::Object* object, Timestamp collected_at, bool is_deleted,
      MetadataCallback metadata_cb) const throw(json::Exception);
  // Given a node object, return the associated metadata.
  MetadataUpdater::ResourceMetadata GetNodeMetadata(
      const json::Object* node, Timestamp collected_at, bool is_deleted) const
      throw(json::Exception);
  // Given a pod object, return the associated metadata.
  MetadataUpdater::ResourceMetadata GetPodMetadata(
      const json::Object* pod, Timestamp collected_at, bool is_deleted) const
      throw(json::Exception);
  // Given a pod object and container info, return the container metadata.
  MetadataUpdater::ResourceMetadata GetContainerMetadata(
      const json::Object* pod, const json::Object* container_spec,
      const json::Object* container_status, Timestamp collected_at,
      bool is_deleted) const throw(json::Exception);
  // Given a pod object and container name, return the legacy resource.
  // The returned "metadata" field will be Metadata::IGNORED.
  MetadataUpdater::ResourceMetadata GetLegacyResource(
      const json::Object* pod, const std::string& container_name) const
      throw(json::Exception);
  // Given a pod object, return the associated pod and container metadata.
  std::vector<MetadataUpdater::ResourceMetadata> GetPodAndContainerMetadata(
      const json::Object* pod, Timestamp collected_at, bool is_deleted) const
      throw(json::Exception);

  // Node Metadata Callback.
  void NodeMetadataCallback(const json::Object* node, IdsAndMR& ids_and_mr)
      const throw(json::Exception);
  // Pod Metadata Callback.
  void PodMetadataCallback(const json::Object* pod, IdsAndMR& ids_and_mr)
      const throw(json::Exception);

  // Gets the Kubernetes master API token.
  // Returns an empty string if unable to find the token.
  const std::string& KubernetesApiToken() const;

  // Gets the Kubernetes namespace for the current container.
  // Returns an empty string if unable to find the namespace.
  const std::string& KubernetesNamespace() const;

  // Cached data.
  mutable std::recursive_mutex mutex_;
  mutable std::string current_node_;
  mutable std::string kubernetes_api_token_;
  mutable std::string kubernetes_namespace_;
  // A memoized map from version to a map from kind to name.
  mutable std::map<std::string, std::map<std::string, std::string>>
      version_to_kind_to_name_;

  const Configuration& config_;
  HealthChecker* health_checker_;
  Environment environment_;
};

class KubernetesUpdater : public PollingMetadataUpdater {
 public:
  KubernetesUpdater(const Configuration& config, HealthChecker* health_checker,
                    MetadataStore* store);
  ~KubernetesUpdater() {
    if (node_watch_thread_.joinable()) {
      node_watch_thread_.join();
    }
    if (pod_watch_thread_.joinable()) {
      pod_watch_thread_.join();
    }
    if (service_watch_thread_.joinable()) {
      service_watch_thread_.join();
    }
    if (endpoints_watch_thread_.joinable()) {
      endpoints_watch_thread_.join();
    }
  }

 protected:
  void ValidateDynamicConfiguration() const throw(ConfigurationValidationError);
  bool ShouldStartUpdater() const;

  void StartUpdater();
  void NotifyStopUpdater();

 private:
  // Metadata watcher callback.
  void MetadataCallback(std::vector<ResourceMetadata>&& result_vector);

  KubernetesReader reader_;
  HealthChecker* health_checker_;
  std::thread node_watch_thread_;
  std::thread pod_watch_thread_;
  std::thread service_watch_thread_;
  std::thread endpoints_watch_thread_;
};

}

#endif  // KUBERNETES_H_
