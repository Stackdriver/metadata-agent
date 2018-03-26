#include "../src/configuration.h"
#include "../src/kubernetes.h"
#include "gtest/gtest.h"

namespace google {

class KubernetesReaderTest : public ::testing::Test {
 protected:
  void InitializeReader() {
    std::stringstream stream("");
    config.ParseConfiguration(stream);

    reader.reset(new KubernetesReader(config));
  }

  json::value ComputePodAssociations(const json::Object* pod)
      const throw(json::Exception) {
    return reader->ComputePodAssociations(pod);
  }

  MetadataAgentConfiguration config;
  std::unique_ptr<KubernetesReader> reader;
};

TEST_F(KubernetesReaderTest, ComputePodAssociationsPodWithoutMetadataThrowsException) {
  InitializeReader();

  try {
    ComputePodAssociations(new json::Object({}));
    FAIL() << "Expected json::Exception";
  } catch(json::Exception const& err) {
    if (err.what().find("There is no metadata") != 0) {
      FAIL() << "Expected an error complaining about the missing key in '" 
             << err.what() << "'";
    }
  }
}

TEST_F(KubernetesReaderTest, ComputePodAssociationsPodWithoutMetadataNamespaceThrowsException) {
  InitializeReader();

  try {
    ComputePodAssociations(new json::Object({
      {"metadata", json::object({})},
    }));
    FAIL() << "Expected json::Exception";
  } catch(json::Exception const& err) {
    if (err.what().find("There is no namespace") != 0) {
      FAIL() << "Expected an error complaining about the missing key in '" 
             << err.what() << "'";
    }
  }
}

TEST_F(KubernetesReaderTest, ComputePodAssociationsPodWithoutMetadataUidThrowsException) {
  InitializeReader();

  try {
    ComputePodAssociations(new json::Object({
      {"metadata", json::object({
        {"namespace", json::string("default")},
      })},
    }));
    FAIL() << "Expected json::Exception";
  } catch(json::Exception const& err) {
    if (err.what().find("There is no uid") != 0) {
      FAIL() << "Expected an error complaining about the missing key in '" 
             << err.what() << "'";
    }
  }
}

} // namespace google
