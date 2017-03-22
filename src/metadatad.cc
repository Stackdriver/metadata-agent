//#include "config.h"

#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <utility>

#include "agent_config.h"
#include "api_server.h"
#include "updater.h"

int main(int ac, char** av) {
  google::MetadataAgentConfiguration config(ac > 1 ? av[1] : "");
  google::MetadataAgent server(config);
  google::PollingMetadataUpdater docker_updater(
      config.DockerUpdaterIntervalSeconds(), &server,
      google::DockerMetadataQuery);

  docker_updater.start();
  server.start();
}
