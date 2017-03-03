//#include "config.h"

#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <utility>

#include "api_server.h"
#include "updater.h"

int main(int ac, char** av) {
  google::MetadataAgent server;
  google::PollingMetadataUpdater docker_updater(3.0, &server,
                                                google::DockerMetadataQuery);

  docker_updater.start();
  server.start();
}
