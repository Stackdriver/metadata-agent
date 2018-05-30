#include "../src/environment.h"
#include "fake_http_server.h"
#include "gtest/gtest.h"

#include <fstream>
#include <sstream>
#include <boost/filesystem.hpp>

namespace google {

class EnvironmentTest : public ::testing::Test {
 protected:
  static void ReadApplicationDefaultCredentials(const Environment& environment) {
    environment.ReadApplicationDefaultCredentials();
  }

  static void SetMetadataServerUrlForTest(Environment* environment,
                                          const std::string& url) {
    environment->SetMetadataServerUrlForTest(url);
  }
};

namespace {

// A file with a given name in a temporary (unique) directory.
boost::filesystem::path TempPath(const std::string& filename) {
  boost::filesystem::path path = boost::filesystem::temp_directory_path();
  path.append(boost::filesystem::unique_path().native());
  path.append(filename);
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

}  // namespace

TEST(TemporaryFile, Basic) {
  boost::filesystem::path path;
  {
    TemporaryFile f("foo", "bar");
    path = f.FullPath();
    EXPECT_TRUE(boost::filesystem::exists(path));
    std::string contents;
    {
      std::ifstream in(path.native());
      in >> contents;
    }
    EXPECT_EQ("bar", contents);
    f.SetContents("xyz");
    {
      std::ifstream in(path.native());
      in >> contents;
    }
    EXPECT_EQ("xyz", contents);
  }
  EXPECT_FALSE(boost::filesystem::exists(path));
}

TEST_F(EnvironmentTest, ReadApplicationDefaultCredentialsSucceeds) {
  TemporaryFile credentials_file(
    std::string(test_info_->name()) + "_creds.json",
    "{\"client_email\":\"user@example.com\",\"private_key\":\"some_key\"}");
  Configuration config(std::istringstream(
      "CredentialsFile: '" + credentials_file.FullPath().native() + "'\n"
  ));
  Environment environment(config);
  EXPECT_NO_THROW(ReadApplicationDefaultCredentials(environment));
  EXPECT_EQ("user@example.com", environment.CredentialsClientEmail());
  EXPECT_EQ("some_key", environment.CredentialsPrivateKey());
}

TEST_F(EnvironmentTest, ReadApplicationDefaultCredentialsCaches) {
  TemporaryFile credentials_file(
    std::string(test_info_->name()) + "_creds.json",
    "{\"client_email\":\"user@example.com\",\"private_key\":\"some_key\"}");
  Configuration config(std::istringstream(
      "CredentialsFile: '" + credentials_file.FullPath().native() + "'\n"
  ));
  Environment environment(config);
  EXPECT_NO_THROW(ReadApplicationDefaultCredentials(environment));
  credentials_file.SetContents(
      "{\"client_email\":\"changed@example.com\",\"private_key\":\"12345\"}"
  );
  EXPECT_EQ("user@example.com", environment.CredentialsClientEmail());
  credentials_file.SetContents(
      "{\"client_email\":\"extra@example.com\",\"private_key\":\"09876\"}"
  );
  EXPECT_EQ("some_key", environment.CredentialsPrivateKey());
}

TEST_F(EnvironmentTest, GetMetadataString) {
  testing::FakeServer server;
  server.SetResponse("/a/b/c", "hello");

  Configuration config;
  Environment environment(config);
  SetMetadataServerUrlForTest(&environment, server.GetUrl());

  EXPECT_EQ("hello", environment.GetMetadataString("a/b/c"));
  EXPECT_EQ("", environment.GetMetadataString("unknown/path"));
}

}  // namespace google
