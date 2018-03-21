#include "../src/resource.h"
#include "../src/json.h"
#include "gtest/gtest.h"

namespace {

TEST(ResourceTest, Type) {
  std::map<std::string, std::string> m;
  google::MonitoredResource mr("some_resource", m);
  EXPECT_EQ("some_resource", mr.type());
}

TEST(ResourceTest, Labels) {
  google::MonitoredResource mr("", {{"foo", "bar"}, {"bar", "baz"}});
  EXPECT_EQ(2, mr.labels().size());
  EXPECT_EQ("bar", mr.labels().at("foo"));
  EXPECT_EQ("baz", mr.labels().at("bar"));
}

TEST(ResourceTest, BasicEquality) {
  google::MonitoredResource mr1("", {});
  google::MonitoredResource mr2("", {});

  EXPECT_EQ(mr1, mr2);
}

TEST(ResourceTest, BasicTypeComparison) {
  google::MonitoredResource mr1("2", {});
  google::MonitoredResource mr2("1", {});

  EXPECT_LT(mr1, mr2);
}

TEST(ResourceTest, BasicLabelComparison) {
  google::MonitoredResource mr1("", {{"b", "b"}});
  google::MonitoredResource mr2("", {{"a", "a"}});

  EXPECT_NE(mr1, mr2);
  EXPECT_LT(mr1, mr2);
}
TEST(ResourceTest, BasicJson) {
  json::value target = json::object({
    {"type", json::string("test")},
    {"labels", json::object({
      {"a", json::string("a")},
      {"b", json::string("b")}
    })}
  });

  google::MonitoredResource mr("test", {{"a", "a"}, {"b", "b"}});

  EXPECT_EQ(target->ToString(), mr.ToJSON()->ToString());
}
}  // namespace
