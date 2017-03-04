#ifndef RESOURCE_H_
#define RESOURCE_H_

//#include "config.h"

#include <map>
#include <ostream>
#include <string>

namespace google {

class MonitoredResource {
 public:
  MonitoredResource(const std::string& type,
                    const std::map<std::string, std::string> labels)
      : type_(type), labels_(labels) {}
  const std::string& type() { return type_; }
  const std::map<std::string, std::string>& labels() { return labels_; }
  bool operator==(const MonitoredResource& other) const {
    return other.type_ == type_ && other.labels_ == labels_;
  }
  bool operator<(const MonitoredResource& other) const {
    return other.type_ < type_
        || (other.type_ == type_ && other.labels_ < labels_);
  }

  std::string ToJSON() const;
  static MonitoredResource FromJSON(const std::string&);

  friend std::ostream& operator<<(std::ostream&, const MonitoredResource&);

 private:
  std::string type_;
  std::map<std::string, std::string> labels_;
};

std::ostream& operator<<(std::ostream&, const MonitoredResource&);

}

#endif  // RESOURCE_H_
