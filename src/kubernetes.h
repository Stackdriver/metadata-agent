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
#include "time.h"
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
  KubernetesReader(const Configuration& config,
                   HealthChecker* health_checker,
                   std::unique_ptr<DelayTimerFactory> delay_timer_factory);
  // A Kubernetes metadata query function.
  std::vector<MetadataUpdater::ResourceMetadata> MetadataQuery() const;

  // Validates the relevant dynamic configuration and throws if it's incorrect.
  void ValidateDynamicConfiguration() const
      throw(MetadataUpdater::ConfigurationValidationError);

  // Node watcher.
  void WatchNodes(const std::string& node_name,
                  MetadataUpdater::UpdateCallback callback) const;

  // Pod watcher.
  void WatchPods(const std::string& node_name,
                 MetadataUpdater::UpdateCallback callback) const;

  // Service watcher.
  void WatchServices(MetadataUpdater::UpdateCallback callback);

  // Endpoints watcher.
  void WatchEndpoints(MetadataUpdater::UpdateCallback callback);

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

  // Returns the full path to the secret filename.
  std::string SecretPath(const std::string& secret) const;

  // Reads a Kubernetes service account secret file into the provided string.
  // Returns true if the file was read successfully.
  bool ReadServiceAccountSecret(
      const std::string& secret, std::string& destination, bool verbose) const;

  // Issues a Kubernetes master API query at a given path and
  // returns a parsed JSON response. The path has to start with "/".
  json::value QueryMaster(const std::string& path) const
      throw(QueryException, json::Exception);

  // Issues a Kubernetes master API query at a given path and
  // watches for parsed JSON responses. The path has to start with "/".
  // Invokes callback for every notification.
  // The name is for logging purposes.
  void WatchMaster(
    const std::string& name,
    const std::string& path,
    std::function<void(const json::Object*, Timestamp, bool)> callback) const
    throw(QueryException, json::Exception);

  // Node watch callback.
  void NodeCallback(
      MetadataUpdater::UpdateCallback callback, const json::Object* node,
      Timestamp collected_at, bool is_deleted) const throw(json::Exception);

  // Pod watch callback.
  void PodCallback(
      MetadataUpdater::UpdateCallback callback, const json::Object* pod,
      Timestamp collected_at, bool is_deleted) const throw(json::Exception);

  // Service watch callback.
  void ServiceCallback(
      MetadataUpdater::UpdateCallback callback, const json::Object* service,
      Timestamp collected_at, bool is_deleted) throw(json::Exception);

  // Endpoints watch callback.
  void EndpointsCallback(
      MetadataUpdater::UpdateCallback callback, const json::Object* endpoints,
      Timestamp collected_at, bool is_deleted) throw(json::Exception);

  // Compute the associations for a given pod.
  json::value ComputePodAssociations(const json::Object* pod) const
      throw(json::Exception);
  // Given a node object, return the associated metadata.
  MetadataUpdater::ResourceMetadata GetNodeMetadata(
      const json::Object* node, Timestamp collected_at, bool is_deleted) const
      throw(json::Exception);
  // Given a pod object, return the associated metadata.
  MetadataUpdater::ResourceMetadata GetPodMetadata(
      const json::Object* pod, json::value associations, Timestamp collected_at,
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
      throw(std::out_of_range, json::Exception);
  // Given a pod object, return the associated pod and container metadata.
  std::vector<MetadataUpdater::ResourceMetadata> GetPodAndContainerMetadata(
      const json::Object* pod, Timestamp collected_at, bool is_deleted) const
      throw(json::Exception);
  // Get a list of service metadata based on the service level caches.
  std::vector<json::value> GetServiceList(
      const std::string& cluster_name, const std::string& location)
      const throw(json::Exception);
  // Return the cluster metadata based on the cached values for
  // service_to_metadta_ and service_to_pods_.
  MetadataUpdater::ResourceMetadata GetClusterMetadata(Timestamp collected_at)
      const throw(json::Exception);

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

  // For a given object, returns the top-level controller object.
  json::value FindTopLevelController(const std::string& ns, json::value object)
      const throw(QueryException, json::Exception);

  // Update service_to_metadata_ cache based on a newly updated service.
  void UpdateServiceToMetadataCache(
      const json::Object* service, bool is_deleted) throw(json::Exception);

  // Update service_to_pods_ cache based on a newly updated endpoints. The
  // Endpoints resource provides a mapping from a single service to its pods:
  // https://kubernetes.io/docs/concepts/services-networking/service/
  void UpdateServiceToPodsCache(
      const json::Object* endpoints, bool is_deleted) throw(json::Exception);

  void SetServiceAccountDirectoryForTest(const std::string& directory) {
    service_account_directory_ = directory;
  }

  // An empty vector value for endpoints that have no pods.
  const std::vector<std::string> kNoPods;

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

  // ServiceKey is a pair of the namespace name and the service name that
  // uniquely identifies a service in a cluster.
  using ServiceKey = std::pair<std::string, std::string>;
  // Mutex for the service related caches.
  mutable std::mutex service_mutex_;
  // Map from service key to service metadata. This map is built based on the
  // response from WatchServices.
  mutable std::map<ServiceKey, json::value> service_to_metadata_;
  // Map from service key to names of pods in the service. This map is built
  // based on the response from WatchEndpoints.
  mutable std::map<ServiceKey, std::vector<std::string>> service_to_pods_;

  const Configuration& config_;
  HealthChecker* health_checker_;
  Environment environment_;
  std::string service_account_directory_;
  std::unique_ptr<DelayTimerFactory> delay_timer_factory_;
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
  KubernetesUpdater(const Configuration& config, HealthChecker* health_checker,
                    MetadataStore* store, std::unique_ptr<DelayTimerFactory>);

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
