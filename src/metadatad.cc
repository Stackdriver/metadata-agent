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

#include <csignal>
#include <cstdlib>
#include <initializer_list>
#include <iostream>

#include "agent.h"
#include "configuration.h"
#include "docker.h"
#include "instance.h"
#include "kubernetes.h"

namespace google {
namespace {

class CleanupState {
 public:
  CleanupState(
      std::initializer_list<MetadataUpdater*> updaters, MetadataAgent* server)
      : updaters_(updaters), server_(server) { server_wait_mutex_.lock(); }

  void StartShutdown() const {
    std::cerr << "Stopping server" << std::endl;
    server_->Stop();
    std::cerr << "Stopping updaters" << std::endl;
    for (MetadataUpdater* updater : updaters_) {
      updater->NotifyStop();
    }
    server_wait_mutex_.unlock();
  }

  void Wait() const {
    std::lock_guard<std::mutex> await_server_shutdown(server_wait_mutex_);
  }

 private:
  mutable std::mutex server_wait_mutex_;
  std::vector<MetadataUpdater*> updaters_;
  MetadataAgent* server_;
};
const CleanupState* cleanup_state;

}  // namespace
}  // google

extern "C" [[noreturn]] void handle_sigterm(int signum) {
  std::cerr << "Caught SIGTERM; shutting down" << std::endl;
  google::cleanup_state->StartShutdown();
  std::cerr << "Exiting" << std::endl;
  std::exit(128 + signum);
}

int main(int ac, char** av) {
  google::Configuration config;
  int parse_result = config.ParseArguments(ac, av);
  if (parse_result) {
    return parse_result < 0 ? 0 : parse_result;
  }

  google::MetadataAgent server(config);

  google::InstanceUpdater instance_updater(config, server.mutable_store());
  google::DockerUpdater docker_updater(config, server.mutable_store());
  google::KubernetesUpdater kubernetes_updater(config, server.health_checker(), server.mutable_store());

  google::cleanup_state = new google::CleanupState(
      {&instance_updater, &docker_updater, &kubernetes_updater},
      &server);
  std::signal(SIGTERM, handle_sigterm);

  instance_updater.Start();
  docker_updater.Start();
  kubernetes_updater.Start();

  server.Start();

  // Wait for the server to shut down.
  google::cleanup_state->Wait();
}
