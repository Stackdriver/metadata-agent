#ifndef RESOURCE_H_
#define RESOURCE_H_

//#include "config.h"

#include <map>
#include <string>

namespace google {

class MonitoredResource {
 public:
  MonitoredResource(const std::string& type,
                    const std::map<std::string, std::string> labels)
      : type_(type), labels_(labels) {}
  const std::string& type() { return type_; }
  const std::map<std::string, std::string>& labels() { return labels_; }
  bool operator==(const MonitoredResource& other) {
    return other.type_ == type_ && other.labels_ == labels_;
  }

  std::string ToJSON();
  static MonitoredResource FromJSON(const std::string&);

 private:
  std::string type_;
  std::map<std::string, std::string> labels_;
};

}

#endif  // RESOURCE_H_
