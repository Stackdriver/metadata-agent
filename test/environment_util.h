#ifndef ENVIRONMENT_UTIL_H_
#define ENVIRONMENT_UTIL_H_

#include "../src/environment.h"

namespace google {
namespace testing {

class EnvironmentUtil {
 public:
  static void SetMetadataServerUrlForTest(Environment* environment,
                                          const std::string& url) {
    environment->SetMetadataServerUrlForTest(url);
  }
};

}  // namespace testing
}  // namespace google

#endif  // ENVIRONMENT_UTIL_H_
