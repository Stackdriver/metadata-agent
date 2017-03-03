#include "resource.h"

namespace google {

std::ostream& operator<<(std::ostream& o, const MonitoredResource& r) {
  o << "{ type: '" << r.type_ << "' labels: {";
  for (const auto& label : r.labels_) {
    o << " " << label.first << ": '" << label.second << "'";
  }
  o << " } }";
}

}
