/*
 * Copyright 2018 Google Inc.
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
#ifndef HEALTH_CHECKER_H_
#define HEALTH_CHECKER_H_

#include <boost/algorithm/string/join.hpp>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <fstream>

#include "configuration.h"

namespace {
  constexpr const char* kHealthStates[] = {
    "kubernetes_pod_thread", "kubernetes_node_thread"
  };
}
namespace google {

class FileWrapper {
 public:
  FileWrapper(const std::string& directory, const std::string& name)
       : directory_(directory), name_(name) {
         Touch(directory_, name_);
  }

  static void Touch(const std::string& directory, const std::string& name) {
    std::string path(boost::algorithm::join(
        std::vector<std::string>{directory, name}, "/"));
    std::ofstream health_file(path);
    health_file << std::endl;
  }

  static void Remove(const std::string& directory, const std::string& name) {
    std::string path(boost::algorithm::join(
        std::vector<std::string>{directory, name}, "/"));
    std::remove(path.c_str());
  }

  static bool Exists(const std::string& directory, const std::string& name) {
    std::string path(boost::algorithm::join(
        std::vector<std::string>{directory, name}, "/"));
    std::ifstream f(path);
    return f.good();
  }

  ~FileWrapper() {
    FileWrapper::Remove(directory_, name_);
  }

  private:
    const std::string& directory_;
    const std::string& name_;
};

// Collects and reports health information about the metadata agent.
class HealthChecker {
 public:
  HealthChecker(const MetadataAgentConfiguration& config);
  void SetUnhealthy(const std::string& state_name);

 private:
  friend class HealthCheckerUnittest;

  bool ReportHealth();
  bool IsHealthy() const;
  std::string MakeHealthCheckPath(const std::string& file_name) const;
  void TouchName(const std::string& state_name);
  bool CheckName(const std::string& state_name) const;
  void Touch(const std::string& path);
  bool Check(const std::string& path) const;
  void InitialCleanup();
  void InitialCreation();

  const MetadataAgentConfiguration& config_;
  std::map<std::string, std::unique_ptr<FileWrapper>> health_files_;
};

}  // namespace google

#endif  // HEALTH_CHECKER_H_
