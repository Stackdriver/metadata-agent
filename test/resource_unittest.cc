/*
 * Copyright 2018 Google Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 **/

#include "../src/resource.h"
#include "../src/json.h"
#include "gtest/gtest.h"

namespace {

TEST(ResourceTest, Type) {
  google::MonitoredResource mr("some_resource", {});
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
  google::MonitoredResource mr1("1", {});
  google::MonitoredResource mr2("2", {});

  EXPECT_LT(mr1, mr2);
}

TEST(ResourceTest, BasicLabelComparison) {
  google::MonitoredResource mr1("", {{"a", "a"}});
  google::MonitoredResource mr2("", {{"b", "b"}});

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
