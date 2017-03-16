#include "resource.h"

#include <sstream>

namespace google {

std::ostream& operator<<(std::ostream& o, const MonitoredResource& r) {
  o << "{ type: '" << r.type_ << "' labels: {";
  for (const auto& label : r.labels_) {
    o << " " << label.first << ": '" << label.second << "'";
  }
  o << " } }";
}

std::unique_ptr<json::Value> MonitoredResource::ToJSON() const {
  std::map<std::string, std::unique_ptr<json::Value>> labels;
  for (const auto& kv : labels_) {
    labels.emplace(kv.first, json::string(kv.second));
  }
  return json::object({
    {"type", json::string(type_)},
    {"labels", json::object(std::move(labels))},
  });
}

}
