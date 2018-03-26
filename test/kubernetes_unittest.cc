#include "../src/configuration.h"
#include "../src/json.h"
#include "../src/kubernetes.h"
#include "gtest/gtest.h"

namespace google {

class KubernetesReaderTest : public ::testing::Test {
 protected:
  void InitializeReader() {
    std::stringstream stream(
      "InstanceId: instance-1\n"
      "InstanceZone: us-east1-d\n");
    config.ParseConfiguration(stream);

    reader.reset(new KubernetesReader(config));
  }

  json::value ComputePodAssociations(const json::Object* pod)
      const throw(json::Exception) {
    return reader->ComputePodAssociations(pod);
  }

  json::value FindTopLevelOwner(
      const std::string& ns, json::value object) const
      throw(KubernetesReader::QueryException, json::Exception) {
    return reader->FindTopLevelOwner(std::move(ns), std::move(object));
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

TEST_F(KubernetesReaderTest, ComputePodAssociationsPodWithoutMetadataNameThrowsException) {
  InitializeReader();

  try {
    ComputePodAssociations(new json::Object({
      {"metadata", json::object({
        {"namespace", json::string("default")},
        {"uid", json::string("12345678")},
      })},
    }));
    FAIL() << "Expected json::Exception";
  } catch(json::Exception const& err) {
    if (err.what().find("There is no name") != 0) {
      FAIL() << "Expected an error complaining about the missing key in '" 
             << err.what() << "'";
    }
  }
}

TEST_F(KubernetesReaderTest, ComputePodAssociationsPodWithoutSpecThrowsException) {
  InitializeReader();

  try {
    ComputePodAssociations(new json::Object({
      {"metadata", json::object({
        {"namespace", json::string("default")},
        {"uid", json::string("12345678")},
        {"name", json::string("some-random-pod")},
      })},
    }));
    FAIL() << "Expected json::Exception";
  } catch(json::Exception const& err) {
    if (err.what().find("There is no spec") != 0) {
      FAIL() << "Expected an error complaining about the missing key in '" 
             << err.what() << "'";
    }
  }
}

TEST_F(KubernetesReaderTest, ComputePodAssociationsPodValidInput) {
  InitializeReader();

  const std::string podName("some-random-pod");

  std::string podAssociations(ComputePodAssociations(new json::Object({
    {"metadata", json::object({
      {"namespace", json::string("default")},
      {"uid", json::string("12345678")},
      {"name", json::string(podName)},
    })},
    {"spec", json::object({})},
  }))->ToString());

  if (podAssociations.find("Pod") == std::string::npos) {
    FAIL() << "Pod should be somewhere in the pod association.";
  }

  if (podAssociations.find(podName) == std::string::npos) {
    FAIL() << "The pod's name should be somewhere in the pod association.";
  }

  if (podAssociations.find("gce_instance") == std::string::npos) {
    FAIL() << "The pod's name should be somewhere in the pod association.";
  }
}

TEST_F(KubernetesReaderTest, FindTopLevelOwnerPodWithoutMetadataThrowsException) {
  InitializeReader();

  try {
    FindTopLevelOwner("default", json::value(new json::Object({})));
    FAIL() << "Expected json::Exception";
  } catch(json::Exception const& err) {
    if (err.what().find("There is no metadata") != 0) {
      FAIL() << "Expected an error complaining about the missing key in '" 
             << err.what() << "'";
    }
  }
}

TEST_F(KubernetesReaderTest, FindTopLevelOwnerPodWithoutOwnerReferenceReturnsInput) {
  InitializeReader();

  json::Object* pod = new json::Object({
    {"metadata", json::object({})},
  });

  json::value topLevelOwner(FindTopLevelOwner("default", json::value(pod)));

  EXPECT_EQ(topLevelOwner->ToString(), pod->ToString());
}

} // namespace google
