//#include "config.h"

#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <utility>

#include "configuration.h"
#include "api_server.h"
#include "updater.h"

int main(int ac, char** av) {
  google::MetadataAgentConfiguration config(ac > 1 ? av[1] : "");
  google::MetadataAgent server(config);
  google::DockerReader docker(config);
  google::PollingMetadataUpdater docker_updater(
      config.DockerUpdaterIntervalSeconds(), &server,
      std::bind(&google::DockerReader::MetadataQuery, &docker));

  docker_updater.start();
  server.start();
}
