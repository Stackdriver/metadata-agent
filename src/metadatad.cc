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
  google::MetadataApiServer server;
  google::DockerMetadataUpdater docker_updater(60.0, &server);

  docker_updater.start();
  server.start();
}
