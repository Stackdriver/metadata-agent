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
#ifndef TEMP_FILE_H_
#define TEMP_FILE_H_

#include <boost/filesystem.hpp>
#include <fstream>
#include <locale>

namespace google {
namespace testing {

// A file with a given name in a temporary (unique) directory.
boost::filesystem::path TempPath(const std::string& filename) {
  boost::filesystem::path path = boost::filesystem::temp_directory_path();
  // Older versions of boost::filesystem::path require a locale.
  const boost::filesystem::path::codecvt_type& codecvt =
     std::use_facet<boost::filesystem::path::codecvt_type>(std::locale(""));
  path.append(boost::filesystem::unique_path().native(), codecvt);
  path.append(filename, codecvt);
  return path;
}

// Creates a file for the lifetime of the object and removes it after.
class TemporaryFile {
 public:
  TemporaryFile(const std::string& filename, const std::string& contents)
      : path_(TempPath(filename)) {
    boost::filesystem::create_directories(path_.parent_path());
    SetContents(contents);
  }
  ~TemporaryFile() {
    boost::filesystem::remove_all(path_.parent_path());
  }
  void SetContents(const std::string& contents) const {
    std::ofstream file(path_.native());
    file << contents << std::flush;
  }
  const boost::filesystem::path& FullPath() const { return path_; }
 private:
  boost::filesystem::path path_;
};

}  // namespace testing
}  // namespace google

#endif  // TEMP_FILE_H_
