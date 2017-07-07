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

#include <boost/network/protocol/http/client.hpp>
#include <chrono>

#include "json.h"
#include "logging.h"
#include "resource.h"
#include "time.h"

namespace http = boost::network::http;

namespace google {

namespace {

#if 0
constexpr const char kubernetes_endpoint_host[] = "https://kubernetes";
#endif
constexpr const char kubernetes_api_version[] = "1.6";
constexpr const char kubernetes_endpoint_path[] = "/api/v1";
constexpr const char resource_type_separator[] = ".";

}

KubernetesReader::KubernetesReader(const MetadataAgentConfiguration& config)
    : config_(config), environment_(config) {}

std::vector<PollingMetadataUpdater::ResourceMetadata>
    KubernetesReader::MetadataQuery() const {
  LOG(INFO) << "Kubernetes Query called";
  const std::string instance_id = environment_.InstanceId();
  const std::string zone = environment_.InstanceZone();
  const std::string cluster_name = environment_.KubernetesClusterName();
  const std::string kubernetes_endpoint(config_.KubernetesEndpointHost() +
                                        kubernetes_endpoint_path);
  const std::string pod_label_selector(
      config_.KubernetesPodLabelSelector().empty()
      ? "" : "?" + config_.KubernetesPodLabelSelector());

  http::client client;
  http::client::request list_request(
      kubernetes_endpoint + "/pods" + pod_label_selector);
  list_request << boost::network::header(
      "Authorization", "Bearer " + environment_.KubernetesApiToken());
  http::client::response list_response = client.get(list_request);
  Timestamp collected_at = std::chrono::system_clock::now();
  LOG(INFO) << "List response: " << body(list_response);
  json::value parsed_list = json::Parser::FromString(body(list_response));
  LOG(INFO) << "Parsed list: " << *parsed_list;
  std::vector<PollingMetadataUpdater::ResourceMetadata> result;
  try {
    const json::Object* podlist_object = parsed_list->As<json::Object>();
    const std::string kind = podlist_object->Get<json::String>("kind");
    const std::string api_version = podlist_object->Get<json::String>("apiVersion");
    const json::Array* pod_list = podlist_object->Get<json::Array>("items");
    for (const json::value& element : *pod_list) {
      try {
        LOG(INFO) << "Pod: " << *element;
        const json::Object* pod = element->As<json::Object>();

        const json::Object* metadata = pod->Get<json::Object>("metadata");
        const std::string namespace_id =
            metadata->Get<json::String>("namespace");
        const std::string pod_name = metadata->Get<json::String>("name");
        const std::string pod_id = metadata->Get<json::String>("uid");
        const std::string created_str =
            metadata->Get<json::String>("creationTimestamp");
        Timestamp created_at = rfc3339::FromString(created_str);

        const json::Object* status = pod->Get<json::Object>("status");
        const std::string started_str = status->Get<json::String>("startTime");
        Timestamp started_at = rfc3339::FromString(started_str);

        //const json::Object* spec = pod->Get<json::Object>("spec");
        //const json::Array* container_list = pod->Get<json::Array>("containers");
        const json::Array* container_list =
            status->Get<json::Array>("containerStatuses");
        for (const json::value& c_element : *container_list) {
          LOG(INFO) << "Container: " << *c_element;
          const json::Object* container = c_element->As<json::Object>();
          const std::string container_name =
              container->Get<json::String>("name");
          const std::string container_id =
              container->Get<json::String>("containerID");
          // TODO: find is_deleted.
          //const json::Object* state = container->Get<json::Object>("state");
          bool is_deleted = false;

          const MonitoredResource resource("gke_container", {
            {"cluster_name", cluster_name},
            {"namespace_id", namespace_id},
            {"instance_id", instance_id},
            {"pod_id", pod_id},
            {"container_name", container_name},
            {"zone", zone},
          });

          const std::string resource_id =
              std::string("gke_container") + resource_type_separator +
              namespace_id + resource_type_separator +
              pod_id + resource_type_separator +
              container_name;
          const std::string resource_name =
              std::string("gke_containerName") + resource_type_separator +
              namespace_id + resource_type_separator +
              pod_name + resource_type_separator +
              container_name;
          result.emplace_back(std::vector<std::string>{resource_id, resource_name},
                              resource,
#ifdef ENABLE_KUBERNETES_METADATA
                              MetadataAgent::Metadata(kubernetes_api_version,
                                                      is_deleted, created_at, collected_at,
                                                      std::move(element->Clone()))
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
  }
  return result;
}

}
