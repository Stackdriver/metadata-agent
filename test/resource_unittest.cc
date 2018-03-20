#include "../src/resource.h"
#include "../src/json.h"
#include "gtest/gtest.h"

namespace {

TEST(ResourceTest, Type) {
  std::map<std::string, std::string> m;
  google::MonitoredResource mr("type", m);
  EXPECT_EQ("type", mr.type());
}

TEST(ResourceTest, Labels) {
  std::map<std::string, std::string> m;
  m["foo"] = "bar";
  m["bar"] = "baz";
  google::MonitoredResource mr("", m);
  EXPECT_EQ(2, mr.labels().size());
  EXPECT_EQ("bar", mr.labels().at("foo"));
  EXPECT_EQ("baz", mr.labels().at("bar"));
}

TEST(ResourceTest, BasicEquality) {
  std::map<std::string, std::string> m1;
  std::map<std::string, std::string> m2;
  google::MonitoredResource mr1("", m1);
  google::MonitoredResource mr2("", m2);

  EXPECT_TRUE(mr1 == mr2);
}

TEST(ResourceTest, BasicTypeComparison) {
  std::map<std::string, std::string> m1;
  std::map<std::string, std::string> m2;
  google::MonitoredResource mr1("2", m1);
  google::MonitoredResource mr2("1", m2);

  EXPECT_TRUE(mr1 < mr2);
}

TEST(ResourceTest, BasicLabelComparison) {
  std::map<std::string, std::string> m1;
  std::map<std::string, std::string> m2;
  m1["b"] = "b";
  m2["a"] = "a";
  google::MonitoredResource mr1("", m1);
  google::MonitoredResource mr2("", m2);

  EXPECT_FALSE(mr1 == mr2);
  EXPECT_TRUE(mr1 < mr2);
}
TEST(ResourceTest, BasicJson) {
  json::value target = json::object({
    {"type", json::string("test")},
    {"labels", json::object({
      {"a", json::string("a")},
      {"b", json::string("b")}
    })}
  });

  std::map<std::string, std::string> m;
  m["a"] = "a";
  m["b"] = "b";
  google::MonitoredResource mr("test", m);

  EXPECT_EQ(target->ToString(), mr.ToJSON()->ToString());
}
} // namespace
