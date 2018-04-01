#include "../src/json.h"
#include "gtest/gtest.h"

#include <functional>
#include <set>
#include <vector>

#define EXPECT_TOSTRING_EQ(s, v) EXPECT_EQ(s, v->ToString())

namespace {

// Verifies that the given json parses, clones, and serializes to its original
// form.
void ExpectSerializesToSelf(const std::string& json_text) {
  json::value v = json::Parser::FromString(json_text);
  EXPECT_TOSTRING_EQ(json_text, v);
  // May as well test a clone while we're at it.
  EXPECT_TOSTRING_EQ(json_text, v->Clone());
}

// If an unexpected exception was thrown, this prints the message in the test
// output.  Otherwise, the test would fail with a cryptic message.
void GuardJsonException(std::function<void()> test) {
  try {
    test();
  } catch (const json::Exception& exc) {
    FAIL() << exc.what();
  }
}


// Type values are distinct.

TEST(Distinct, Types) {
  std::vector<json::Type> all_types({
    json::NullType, json::BooleanType, json::NumberType, json::StringType,
    json::ArrayType, json::ObjectType
  });
  std::set<json::Type> all_types_set(all_types.begin(), all_types.end());
  EXPECT_EQ(all_types.size(), all_types_set.size());
}


// Trival null.

TEST(TrivialParseTest, Null) {
  GuardJsonException([](){
    json::value v = json::Parser::FromString("null");
    EXPECT_EQ(json::NullType, v->type());

    EXPECT_TRUE(v->Is<json::Null>());
    EXPECT_FALSE(v->Is<json::Boolean>());
    EXPECT_FALSE(v->Is<json::Number>());
    EXPECT_FALSE(v->Is<json::String>());
    EXPECT_FALSE(v->Is<json::Array>());
    EXPECT_FALSE(v->Is<json::Object>());

    const auto& null_value = v->As<json::Null>();
    EXPECT_THROW(v->As<json::Boolean>(), json::Exception);
    EXPECT_THROW(v->As<json::Number>(), json::Exception);
    EXPECT_THROW(v->As<json::String>(), json::Exception);
    EXPECT_THROW(v->As<json::Array>(), json::Exception);
    EXPECT_THROW(v->As<json::Object>(), json::Exception);

    EXPECT_EQ(json::NullType, null_value->type());
  });
}

TEST(TrivialToStringTest, Null) {
  GuardJsonException([](){
    EXPECT_TOSTRING_EQ("null", json::null());
  });
}


// Trival boolean.

TEST(TrivialParseTest, True) {
  GuardJsonException([](){
    json::value v = json::Parser::FromString("true");
    EXPECT_EQ(json::BooleanType, v->type());

    EXPECT_FALSE(v->Is<json::Null>());
    EXPECT_TRUE(v->Is<json::Boolean>());
    EXPECT_FALSE(v->Is<json::Number>());
    EXPECT_FALSE(v->Is<json::String>());
    EXPECT_FALSE(v->Is<json::Array>());
    EXPECT_FALSE(v->Is<json::Object>());

    EXPECT_THROW(v->As<json::Null>(), json::Exception);
    const auto& boolean = v->As<json::Boolean>();
    EXPECT_THROW(v->As<json::Number>(), json::Exception);
    EXPECT_THROW(v->As<json::String>(), json::Exception);
    EXPECT_THROW(v->As<json::Array>(), json::Exception);
    EXPECT_THROW(v->As<json::Object>(), json::Exception);

    EXPECT_EQ(true, boolean->value());
  });
}

TEST(TrivialToStringTest, True) {
  GuardJsonException([](){
    EXPECT_TOSTRING_EQ("true", json::boolean(true));
  });
}

TEST(TrivialParseTest, False) {
  GuardJsonException([](){
    json::value v = json::Parser::FromString("false");
    EXPECT_EQ(json::BooleanType, v->type());

    EXPECT_FALSE(v->Is<json::Null>());
    EXPECT_TRUE(v->Is<json::Boolean>());
    EXPECT_FALSE(v->Is<json::Number>());
    EXPECT_FALSE(v->Is<json::String>());
    EXPECT_FALSE(v->Is<json::Array>());
    EXPECT_FALSE(v->Is<json::Object>());

    EXPECT_THROW(v->As<json::Null>(), json::Exception);
    const auto& boolean = v->As<json::Boolean>();
    EXPECT_THROW(v->As<json::Number>(), json::Exception);
    EXPECT_THROW(v->As<json::String>(), json::Exception);
    EXPECT_THROW(v->As<json::Array>(), json::Exception);
    EXPECT_THROW(v->As<json::Object>(), json::Exception);

    EXPECT_EQ(false, boolean->value());
  });
}

TEST(TrivialToStringTest, False) {
  GuardJsonException([](){
    EXPECT_TOSTRING_EQ("false", json::boolean(false));
  });
}


// Trival number.

TEST(TrivialParseTest, Number) {
  GuardJsonException([](){
    json::value v = json::Parser::FromString("2");
    EXPECT_EQ(json::NumberType, v->type());

    EXPECT_FALSE(v->Is<json::Null>());
    EXPECT_FALSE(v->Is<json::Boolean>());
    EXPECT_TRUE(v->Is<json::Number>());
    EXPECT_FALSE(v->Is<json::String>());
    EXPECT_FALSE(v->Is<json::Array>());
    EXPECT_FALSE(v->Is<json::Object>());

    EXPECT_THROW(v->As<json::Null>(), json::Exception);
    EXPECT_THROW(v->As<json::Boolean>(), json::Exception);
    const auto& num = v->As<json::Number>();
    EXPECT_THROW(v->As<json::String>(), json::Exception);
    EXPECT_THROW(v->As<json::Array>(), json::Exception);
    EXPECT_THROW(v->As<json::Object>(), json::Exception);

    EXPECT_EQ(2.0, num->value());
  });
}

TEST(TrivialToStringTest, Number) {
  GuardJsonException([](){
    EXPECT_TOSTRING_EQ("2.0", json::number(2.0));
  });
}


// Trival string.

TEST(TrivialParseTest, StringEmpty) {
  GuardJsonException([](){
    json::value v = json::Parser::FromString("\"\"");
    EXPECT_EQ(json::StringType, v->type());

    EXPECT_FALSE(v->Is<json::Null>());
    EXPECT_FALSE(v->Is<json::Boolean>());
    EXPECT_FALSE(v->Is<json::Number>());
    EXPECT_TRUE(v->Is<json::String>());
    EXPECT_FALSE(v->Is<json::Array>());
    EXPECT_FALSE(v->Is<json::Object>());

    EXPECT_THROW(v->As<json::Null>(), json::Exception);
    EXPECT_THROW(v->As<json::Boolean>(), json::Exception);
    EXPECT_THROW(v->As<json::Number>(), json::Exception);
    const auto& str = v->As<json::String>();
    EXPECT_THROW(v->As<json::Array>(), json::Exception);
    EXPECT_THROW(v->As<json::Object>(), json::Exception);

    EXPECT_EQ("", str->value());
  });
}

TEST(TrivialToStringTest, StringEmpty) {
  GuardJsonException([](){
    EXPECT_TOSTRING_EQ("\"\"", json::string(""));
  });
}

TEST(TrivialParseTest, StringOneChar) {
  GuardJsonException([](){
    json::value v = json::Parser::FromString("\"x\"");
    EXPECT_EQ(json::StringType, v->type());

    EXPECT_FALSE(v->Is<json::Null>());
    EXPECT_FALSE(v->Is<json::Boolean>());
    EXPECT_FALSE(v->Is<json::Number>());
    EXPECT_TRUE(v->Is<json::String>());
    EXPECT_FALSE(v->Is<json::Array>());
    EXPECT_FALSE(v->Is<json::Object>());

    EXPECT_THROW(v->As<json::Null>(), json::Exception);
    EXPECT_THROW(v->As<json::Boolean>(), json::Exception);
    EXPECT_THROW(v->As<json::Number>(), json::Exception);
    const auto& str = v->As<json::String>();
    EXPECT_THROW(v->As<json::Array>(), json::Exception);
    EXPECT_THROW(v->As<json::Object>(), json::Exception);

    EXPECT_EQ("x", str->value());
  });
}

TEST(TrivialToStringTest, StringOneChar) {
  GuardJsonException([](){
    EXPECT_TOSTRING_EQ("\"x\"", json::string("x"));
  });
}


// Trival array.

TEST(TrivialParseTest, ArrayEmpty) {
  GuardJsonException([](){
    json::value v = json::Parser::FromString("[]");
    EXPECT_EQ(json::ArrayType, v->type());

    EXPECT_FALSE(v->Is<json::Null>());
    EXPECT_FALSE(v->Is<json::Boolean>());
    EXPECT_FALSE(v->Is<json::Number>());
    EXPECT_FALSE(v->Is<json::String>());
    EXPECT_TRUE(v->Is<json::Array>());
    EXPECT_FALSE(v->Is<json::Object>());

    EXPECT_THROW(v->As<json::Null>(), json::Exception);
    EXPECT_THROW(v->As<json::Boolean>(), json::Exception);
    EXPECT_THROW(v->As<json::Number>(), json::Exception);
    EXPECT_THROW(v->As<json::String>(), json::Exception);
    const auto& arr = v->As<json::Array>();
    EXPECT_THROW(v->As<json::Object>(), json::Exception);

    EXPECT_TRUE(arr->empty());
    EXPECT_EQ(0, arr->size());
  });
}

TEST(TrivialToStringTest, ArrayEmpty) {
  GuardJsonException([](){
    EXPECT_TOSTRING_EQ("[]", json::array({}));
  });
}

TEST(TrivialParseTest, ArrayOneElement) {
  GuardJsonException([](){
    json::value v = json::Parser::FromString("[2]");
    EXPECT_EQ(json::ArrayType, v->type());

    EXPECT_FALSE(v->Is<json::Null>());
    EXPECT_FALSE(v->Is<json::Boolean>());
    EXPECT_FALSE(v->Is<json::Number>());
    EXPECT_FALSE(v->Is<json::String>());
    EXPECT_TRUE(v->Is<json::Array>());
    EXPECT_FALSE(v->Is<json::Object>());

    EXPECT_THROW(v->As<json::Null>(), json::Exception);
    EXPECT_THROW(v->As<json::Boolean>(), json::Exception);
    EXPECT_THROW(v->As<json::Number>(), json::Exception);
    EXPECT_THROW(v->As<json::String>(), json::Exception);
    const auto& arr = v->As<json::Array>();
    EXPECT_THROW(v->As<json::Object>(), json::Exception);

    EXPECT_FALSE(arr->empty());
    EXPECT_EQ(1, arr->size());
    EXPECT_EQ(2.0, (*arr)[0]->As<json::Number>()->value());
    EXPECT_EQ(2.0, arr->at(0)->As<json::Number>()->value());
  });
}

TEST(TrivialToStringTest, ArrayOneElement) {
  GuardJsonException([](){
    EXPECT_TOSTRING_EQ("[2.0]", json::array({json::number(2.0)}));
  });
}


// Trival object.

TEST(TrivialParseTest, ObjectEmpty) {
  GuardJsonException([](){
    json::value v = json::Parser::FromString("{}");
    EXPECT_EQ(json::ObjectType, v->type());

    EXPECT_FALSE(v->Is<json::Null>());
    EXPECT_FALSE(v->Is<json::Boolean>());
    EXPECT_FALSE(v->Is<json::Number>());
    EXPECT_FALSE(v->Is<json::String>());
    EXPECT_FALSE(v->Is<json::Array>());
    EXPECT_TRUE(v->Is<json::Object>());

    EXPECT_THROW(v->As<json::Null>(), json::Exception);
    EXPECT_THROW(v->As<json::Boolean>(), json::Exception);
    EXPECT_THROW(v->As<json::Number>(), json::Exception);
    EXPECT_THROW(v->As<json::String>(), json::Exception);
    EXPECT_THROW(v->As<json::Array>(), json::Exception);
    const auto& obj = v->As<json::Object>();

    EXPECT_TRUE(obj->empty());
    EXPECT_THROW(obj->Get<json::Number>("g"), json::Exception);
    EXPECT_THROW(obj->at("g"), std::out_of_range);
  });
}

TEST(TrivialToStringTest, ObjectEmpty) {
  GuardJsonException([](){
    EXPECT_TOSTRING_EQ("{}", json::object({}));
  });
}

TEST(TrivialParseTest, ObjectOneField) {
  GuardJsonException([](){
    json::value v = json::Parser::FromString("{\"f\":2}");
    EXPECT_EQ(json::ObjectType, v->type());

    EXPECT_FALSE(v->Is<json::Null>());
    EXPECT_FALSE(v->Is<json::Boolean>());
    EXPECT_FALSE(v->Is<json::Number>());
    EXPECT_FALSE(v->Is<json::String>());
    EXPECT_FALSE(v->Is<json::Array>());
    EXPECT_TRUE(v->Is<json::Object>());

    EXPECT_THROW(v->As<json::Null>(), json::Exception);
    EXPECT_THROW(v->As<json::Boolean>(), json::Exception);
    EXPECT_THROW(v->As<json::Number>(), json::Exception);
    EXPECT_THROW(v->As<json::String>(), json::Exception);
    EXPECT_THROW(v->As<json::Array>(), json::Exception);
    const auto& obj = v->As<json::Object>();

    EXPECT_EQ(1, obj->size());
    EXPECT_TRUE(obj->Has("f"));
    EXPECT_THROW(obj->Get<json::Null>("f"), json::Exception);
    EXPECT_THROW(obj->Get<json::Boolean>("f"), json::Exception);
    EXPECT_EQ(2.0, obj->Get<json::Number>("f"));
    EXPECT_THROW(obj->Get<json::String>("f"), json::Exception);
    EXPECT_THROW(obj->Get<json::Array>("f"), json::Exception);
    EXPECT_THROW(obj->Get<json::Object>("f"), json::Exception);
    EXPECT_EQ(2.0, obj->at("f")->As<json::Number>()->value());

    EXPECT_FALSE(obj->Has("g"));
    EXPECT_THROW(obj->Get<json::Number>("g"), json::Exception);
    EXPECT_THROW(obj->at("g"), std::out_of_range);
  });
}

TEST(TrivialToStringTest, ObjectOneField) {
  GuardJsonException([](){
    json::value v = json::object({
      {"f", json::number(2.0)},
    });
    EXPECT_TOSTRING_EQ("{\"f\":2.0}", v);
  });
}


// Parse methods.

TEST(AllFromStreamTest, SingleObject) {
  GuardJsonException([](){
    std::vector<json::value> v = json::Parser::AllFromStream(std::istringstream(
      "{\n"
      "  \"foo\": [1, 2, 3],\n"
      "  \"bar\": {\"x\": 0, \"y\": null},\n"
      "  \"baz\": true,\n"
      "  \"str\": \"asdfasdf\"\n"
      "}\n"
    ));
    EXPECT_EQ(1, v.size());
    EXPECT_TOSTRING_EQ(
      "{"
      "\"bar\":{\"x\":0.0,\"y\":null},"
      "\"baz\":true,"
      "\"foo\":[1.0,2.0,3.0],"
      "\"str\":\"asdfasdf\""
      "}",
      v[0]);
  });
}

TEST(AllFromStreamTest, MultipleObjects) {
  GuardJsonException([](){
    std::vector<json::value> v = json::Parser::AllFromStream(std::istringstream(
      "{\n"
      "  \"foo\": [1, 2, 3],\n"
      "  \"bar\": {\"x\": 0, \"y\": null},\n"
      "  \"baz\": true,\n"
      "  \"str\": \"asdfasdf\"\n"
      "}\n"
      "{\n"
      "  \"foo1\": [1, 2, 3],\n"
      "  \"bar1\": {\"x\": 0, \"y\": null},\n"
      "  \"baz1\": true,\n"
      "  \"str1\": \"asdfasdf\"\n"
      "}\n"
    ));
    EXPECT_EQ(2, v.size());
    EXPECT_TOSTRING_EQ(
      "{"
      "\"bar\":{\"x\":0.0,\"y\":null},"
      "\"baz\":true,"
      "\"foo\":[1.0,2.0,3.0],"
      "\"str\":\"asdfasdf\""
      "}",
      v[0]);
    EXPECT_TOSTRING_EQ(
      "{"
      "\"bar1\":{\"x\":0.0,\"y\":null},"
      "\"baz1\":true,"
      "\"foo1\":[1.0,2.0,3.0],"
      "\"str1\":\"asdfasdf\""
      "}",
      v[1]);
  });
}

TEST(AllFromStringTest, SingleObject) {
  GuardJsonException([](){
    std::vector<json::value> v = json::Parser::AllFromString(
      "{\n"
      "  \"foo\": [1, 2, 3],\n"
      "  \"bar\": {\"x\": 0, \"y\": null},\n"
      "  \"baz\": true,\n"
      "  \"str\": \"asdfasdf\"\n"
      "}\n"
    );
    EXPECT_EQ(1, v.size());
    EXPECT_TOSTRING_EQ(
      "{"
      "\"bar\":{\"x\":0.0,\"y\":null},"
      "\"baz\":true,"
      "\"foo\":[1.0,2.0,3.0],"
      "\"str\":\"asdfasdf\""
      "}",
      v[0]);
  });
}

TEST(AllFromStringTest, MultipleObjects) {
  GuardJsonException([](){
    std::vector<json::value> v = json::Parser::AllFromString(
      "{\n"
      "  \"foo\": [1, 2, 3],\n"
      "  \"bar\": {\"x\": 0, \"y\": null},\n"
      "  \"baz\": true,\n"
      "  \"str\": \"asdfasdf\"\n"
      "}\n"
      "{\n"
      "  \"foo1\": [1, 2, 3],\n"
      "  \"bar1\": {\"x\": 0, \"y\": null},\n"
      "  \"baz1\": true,\n"
      "  \"str1\": \"asdfasdf\"\n"
      "}\n"
    );
    EXPECT_EQ(2, v.size());
    EXPECT_TOSTRING_EQ(
      "{"
      "\"bar\":{\"x\":0.0,\"y\":null},"
      "\"baz\":true,"
      "\"foo\":[1.0,2.0,3.0],"
      "\"str\":\"asdfasdf\""
      "}",
      v[0]);
    EXPECT_TOSTRING_EQ(
      "{"
      "\"bar1\":{\"x\":0.0,\"y\":null},"
      "\"baz1\":true,"
      "\"foo1\":[1.0,2.0,3.0],"
      "\"str1\":\"asdfasdf\""
      "}",
      v[1]);
  });
}

TEST(FromStreamTest, Complete) {
  GuardJsonException([](){
    json::value v = json::Parser::FromStream(std::istringstream(
      "{\n"
      "  \"foo\": [1, 2, 3],\n"
      "  \"bar\": {\"x\": 0, \"y\": null},\n"
      "  \"baz\": true,\n"
      "  \"str\": \"asdfasdf\"\n"
      "}\n"
    ));
    EXPECT_TOSTRING_EQ(
      "{"
      "\"bar\":{\"x\":0.0,\"y\":null},"
      "\"baz\":true,"
      "\"foo\":[1.0,2.0,3.0],"
      "\"str\":\"asdfasdf\""
      "}",
      v);
  });
}

TEST(FromStreamTest, ExpectSingle) {
  EXPECT_THROW(json::Parser::FromStream(std::istringstream(
    "{\n"
    "  \"foo\": [1, 2, 3],\n"
    "  \"bar\": {\"x\": 0, \"y\": null},\n"
    "  \"baz\": true,\n"
    "  \"str\": \"asdfasdf\"\n"
    "}\n"
    "{\n"
    "  \"foo1\": [1, 2, 3],\n"
    "  \"bar1\": {\"x\": 0, \"y\": null},\n"
    "  \"baz1\": true,\n"
    "  \"str1\": \"asdfasdf\"\n"
    "}\n"
  )), json::Exception);
}

TEST(FromStringTest, Complete) {
  GuardJsonException([](){
    json::value v = json::Parser::FromString(
      "{\n"
      "  \"foo\": [1, 2, 3],\n"
      "  \"bar\": {\"x\": 0, \"y\": null},\n"
      "  \"baz\": true,\n"
      "  \"str\": \"asdfasdf\"\n"
      "}\n"
    );
    EXPECT_TOSTRING_EQ(
      "{"
      "\"bar\":{\"x\":0.0,\"y\":null},"
      "\"baz\":true,"
      "\"foo\":[1.0,2.0,3.0],"
      "\"str\":\"asdfasdf\""
      "}",
      v);
  });
}

TEST(FromStringTest, ExpectSingle) {
  EXPECT_THROW(json::Parser::FromString(
    "{\n"
    "  \"foo\": [1, 2, 3],\n"
    "  \"bar\": {\"x\": 0, \"y\": null},\n"
    "  \"baz\": true,\n"
    "  \"str\": \"asdfasdf\"\n"
    "}\n"
    "{\n"
    "  \"foo1\": [1, 2, 3],\n"
    "  \"bar1\": {\"x\": 0, \"y\": null},\n"
    "  \"baz1\": true,\n"
    "  \"str1\": \"asdfasdf\"\n"
    "}\n"
  ), json::Exception);
}


// String edge cases.

#if 0
// TODO: yajl doesn't support newlines in strings.
// (https://github.com/lloyd/yajl/issues/180)
TEST(EdgeTest, StringWithNewline) {
  GuardJsonException([](){
    json::value v = json::Parser::FromString("\"\n\"");
    EXPECT_EQ("\n", v->As<json::String>()->value());
    // Output should be escaped with \n as that is more canonical.
    EXPECT_TOSTRING_EQ("\"\n\"", v);
  });
}
#endif

TEST(EdgeTest, StringWithNonUnicodeEscape) {
  GuardJsonException([](){
    json::value v = json::Parser::FromString("\"foo\\nbar\"");
    EXPECT_EQ("foo\nbar", v->As<json::String>()->value());
    // Output should be escaped as that is more canonical.
    EXPECT_TOSTRING_EQ("\"foo\\nbar\"", v);
  });
}

TEST(EdgeTest, StringWithEveryEscape) {
  GuardJsonException([](){
    json::value v = json::Parser::FromString("\"x\\\\x\\/x\\bx\\fx\\nx\\rx\\tx\"");
    EXPECT_EQ("x\\x/x\bx\fx\nx\rx\tx", v->As<json::String>()->value());
    // Output should be escaped as that is more canonical.
    // Since '/' does not need to be escaped, output does not escape that.
    EXPECT_TOSTRING_EQ("\"x\\\\x/x\\bx\\fx\\nx\\rx\\tx\"", v);
  });
}

TEST(EdgeTest, StringWithUnicodeEscape) {
  GuardJsonException([](){
    json::value v = json::Parser::FromString("\"foo\\u000abar\"");
    EXPECT_EQ("foo\nbar", v->As<json::String>()->value());
    // Output should be escaped as that is more canonical.
    EXPECT_TOSTRING_EQ("\"foo\\nbar\"", v);
  });
}

TEST(EdgeTest, StringWithUTF8) {
  GuardJsonException([](){
    // This is Korean for "Hello World!".
    json::value v = json::Parser::FromString("\"안녕 세상아!\"");
    EXPECT_EQ("안녕 세상아!", v->As<json::String>()->value());
    EXPECT_TOSTRING_EQ("\"안녕 세상아!\"", v);
  });
}


// Number edge cases.

TEST(EdgeTest, PositiveNumbers) {
  GuardJsonException([](){
    json::value v = json::Parser::FromString(
      "[\n"
      "  0, 0e+0, 0e-0, 0e0, 0E+0, 0E-0, 0E0,\n"
      "  0.0, 0.0e+0, 0.0e-0, 0.0e0, 0.0E+0, 0.0E-0, 0.0E0,\n"
      "  10, 10e+0, 10e-0, 10e0, 10E+0, 10E-0, 10E0,\n"
      "  10.0, 10.0e+0, 10.0e-0, 10.0e0, 10.0E+0, 10.0E-0, 10.0E0,\n"
      "  123, 123.456, 789, 1.05, 1.999e-99\n"
      "]"
    );
    std::vector<double> numbers;
    for (const auto& number : *v->As<json::Array>()) {
      numbers.push_back(number->As<json::Number>()->value());
    }
    EXPECT_EQ(
      std::vector<double>({
        0, 0e+0, 0e-0, 0e0, 0E+0, 0E-0, 0E0,
        0.0, 0.0e+0, 0.0e-0, 0.0e0, 0.0E+0, 0.0E-0, 0.0E0,
        10, 10e+0, 10e-0, 10e0, 10E+0, 10E-0, 10E0,
        10.0, 10.0e+0, 10.0e-0, 10.0e0, 10.0E+0, 10.0E-0, 10.0E0,
        123, 123.456, 789, 1.05, 1.999e-99,
      }),
      numbers);
    EXPECT_TOSTRING_EQ(
      "["
        "0.0,0.0,0.0,0.0,0.0,0.0,0.0,"
        "0.0,0.0,0.0,0.0,0.0,0.0,0.0,"
        "10.0,10.0,10.0,10.0,10.0,10.0,10.0,"
        "10.0,10.0,10.0,10.0,10.0,10.0,10.0,"
        "123.0,123.45600000000000307,789.0,"
        "1.0500000000000000444,1.9989999999999998999e-99"
      "]",
      v);
  });
}

TEST(EdgeTest, NegativeNumbers) {
  GuardJsonException([](){
    json::value v = json::Parser::FromString(
      "[\n"
      "  -0, -0e+0, -0e-0, -0e0, -0E+0, -0E-0, -0E0,\n"
      "  -0.0, -0.0e+0, -0.0e-0, -0.0e0, -0.0E+0, -0.0E-0, -0.0E0,\n"
      "  -10, -10e+0, -10e-0, -10e0, -10E+0, -10E-0, -10E0,\n"
      "  -10.0, -10.0e+0, -10.0e-0, -10.0e0, -10.0E+0, -10.0E-0, -10.0E0,\n"
      "  -123, -123.456, -789, -1.05, -1.999e-99\n"
      "]"
    );
    std::vector<double> numbers;
    for (const auto& number : *v->As<json::Array>()) {
      numbers.push_back(number->As<json::Number>()->value());
    }
    EXPECT_EQ(
      std::vector<double>({
        -0, -0e+0, -0e-0, -0e0, -0E+0, -0E-0, -0E0,
        -0.0, -0.0e+0, -0.0e-0, -0.0e0, -0.0E+0, -0.0E-0, -0.0E0,
        -10, -10e+0, -10e-0, -10e0, -10E+0, -10E-0, -10E0,
        -10.0, -10.0e+0, -10.0e-0, -10.0e0, -10.0E+0, -10.0E-0, -10.0E0,
        -123, -123.456, -789, -1.05, -1.999e-99,
      }),
      numbers);
    // TODO(igorp): This first one looks spurious.
    EXPECT_TOSTRING_EQ(
      "["
        "0.0,-0.0,-0.0,-0.0,-0.0,-0.0,-0.0,"
        "-0.0,-0.0,-0.0,-0.0,-0.0,-0.0,-0.0,"
        "-10.0,-10.0,-10.0,-10.0,-10.0,-10.0,-10.0,"
        "-10.0,-10.0,-10.0,-10.0,-10.0,-10.0,-10.0,"
        "-123.0,-123.45600000000000307,-789.0,"
        "-1.0500000000000000444,-1.9989999999999998999e-99"
      "]",
      v);
  });
}


// Big tests.

TEST(BigTest, RealisticConstruction) {
  GuardJsonException([](){
    json::value v = json::object({
      {"foo", json::array({json::number(1), json::number(2), json::number(3)})},
      {"bar", json::object({{"x", json::number(0)}, {"y", json::null()}})},
      {"baz", json::boolean(true)},
      {"str", json::string("asdfasdf")},
    });
    EXPECT_TOSTRING_EQ(
      "{"
      "\"bar\":{\"x\":0.0,\"y\":null},"
      "\"baz\":true,"
      "\"foo\":[1.0,2.0,3.0],"
      "\"str\":\"asdfasdf\""
      "}",
      v);
  });
}

TEST(BigTest, Clone) {
  GuardJsonException([](){
    json::value v = json::object({
      {"foo", json::array({json::number(1), json::number(2), json::number(3)})},
      {"bar", json::object({{"x", json::number(0)}, {"y", json::null()}})},
      {"baz", json::boolean(true)},
      {"str", json::string("asdfasdf")},
    });
    json::value cloned = v->Clone();
    EXPECT_TOSTRING_EQ(
      "{"
      "\"bar\":{\"x\":0.0,\"y\":null},"
      "\"baz\":true,"
      "\"foo\":[1.0,2.0,3.0],"
      "\"str\":\"asdfasdf\""
      "}",
      cloned);
  });
}

TEST(BigTest, LotsOfArrayNesting) {
  GuardJsonException([](){
    ExpectSerializesToSelf(
      "[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[["
      "]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]");
  });
}

TEST(BigTest, LotsOfObjectNesting) {
  GuardJsonException([](){
    ExpectSerializesToSelf(
      "{\"f\":{\"f\":{\"f\":{\"f\":{\"f\":{\"f\":{\"f\":{\"f\":{\"f\":{\"f\":"
      "{\"f\":{\"f\":{\"f\":{\"f\":{\"f\":{\"f\":{\"f\":{\"f\":{\"f\":{\"f\":"
      "{\"f\":{\"f\":{\"f\":{\"f\":{\"f\":{\"f\":{\"f\":{\"f\":{\"f\":{\"f\":"
      "{\"f\":{\"f\":{\"f\":{\"f\":{\"f\":{\"f\":{\"f\":{\"f\":null"
      "}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}");
  });
}


// Parse errors.

TEST(ParseError, Empty) {
  EXPECT_THROW(json::Parser::FromString(""), json::Exception);
}

TEST(ParseError, UnexpectedChar) {
  EXPECT_THROW(json::Parser::FromString("x"), json::Exception);
}

TEST(ParseError, UnterminatedArray) {
  EXPECT_THROW(json::Parser::FromString("["), json::Exception);
}

TEST(ParseError, UnmatchedArrayClose) {
  EXPECT_THROW(json::Parser::FromString("]"), json::Exception);
}

TEST(ParseError, UnterminatedObject) {
  EXPECT_THROW(json::Parser::FromString("{"), json::Exception);
}

TEST(ParseError, UnmatchedObjectClose) {
  EXPECT_THROW(json::Parser::FromString("}"), json::Exception);
}

TEST(ParseError, UnterminatedString) {
  EXPECT_THROW(json::Parser::FromString("\""), json::Exception);
}

TEST(ParseError, UnterminatedEscape) {
  EXPECT_THROW(json::Parser::FromString("\"\\\""), json::Exception);
}

TEST(ParseError, UnterminatedUnicodeEscape) {
  EXPECT_THROW(json::Parser::FromString("\"\\u\""), json::Exception);
  EXPECT_THROW(json::Parser::FromString("\"\\u0\""), json::Exception);
  EXPECT_THROW(json::Parser::FromString("\"\\u00\""), json::Exception);
  EXPECT_THROW(json::Parser::FromString("\"\\u000\""), json::Exception);
}

TEST(ParseError, ObjectNoValue) {
  EXPECT_THROW(json::Parser::FromString("{\"x\"}"), json::Exception);
  EXPECT_THROW(json::Parser::FromString("{\"x\":}"), json::Exception);
}

// Streaming parsing test.

TEST(StreamingTest, Complete) {
  GuardJsonException([](){
    json::value v;
    json::Parser p([&v](json::value r) { v = std::move(r); });
    p.ParseStream(std::istringstream(
      "{\n"
      "  \"foo\": [1, 2, 3],\n"
      "  \"bar\": {\"x\": 0, \"y\": null},\n"
      "  \"baz\": true,\n"
      "  \"str\": \"asdfasdf\"\n"
      "}\n"
    ));
    EXPECT_TOSTRING_EQ(
      "{"
      "\"bar\":{\"x\":0.0,\"y\":null},"
      "\"baz\":true,"
      "\"foo\":[1.0,2.0,3.0],"
      "\"str\":\"asdfasdf\""
      "}",
      v);
  });
}

TEST(StreamingTest, Split) {
  GuardJsonException([](){
    json::value v;
    json::Parser p([&v](json::value r) { v = std::move(r); });
    p.ParseStream(std::istringstream(
      "{\n"
      "  \"foo\": [1, 2, 3],\n"
    ));
    p.ParseStream(std::istringstream(
      "  \"bar\": {\"x\": 0, \"y\": null},\n"
      "  \"baz\": true,\n"
    ));
    p.ParseStream(std::istringstream(
      "  \"str\": \"asdfasdf\"\n"
      "}\n"
    ));
    EXPECT_TOSTRING_EQ(
      "{"
      "\"bar\":{\"x\":0.0,\"y\":null},"
      "\"baz\":true,"
      "\"foo\":[1.0,2.0,3.0],"
      "\"str\":\"asdfasdf\""
      "}",
      v);
  });
}

TEST(StreamingTest, SplitMidValue) {
  GuardJsonException([](){
    json::value v;
    json::Parser p([&v](json::value r) { v = std::move(r); });
    p.ParseStream(std::istringstream(
      "{\n"
      "  \"foo\": [1, 2, 3],\n"
      "  \"ba"
    ));
    p.ParseStream(std::istringstream(
            "r\": {\"x\": 0, \"y\": nu"
    ));
    p.ParseStream(std::istringstream(
                                     "ll},\n"
      "  \"baz\": true,\n"
      "  \"str\""
    ));
    p.ParseStream(std::istringstream(
               ": \"asdfasdf\"\n"
      "}\n"
    ));
    EXPECT_TOSTRING_EQ(
      "{"
      "\"bar\":{\"x\":0.0,\"y\":null},"
      "\"baz\":true,"
      "\"foo\":[1.0,2.0,3.0],"
      "\"str\":\"asdfasdf\""
      "}",
      v);
  });
}

TEST(StreamingTest, MultipleObjects) {
  GuardJsonException([](){
    std::vector<json::value> v;
    json::Parser p([&v](json::value r) { v.emplace_back(std::move(r)); });
    p.ParseStream(std::istringstream(
      "{\n"
      "  \"foo\": [1, 2, 3],\n"
      "  \"bar\": {\"x\": 0, \"y\": null},\n"
      "  \"baz\": true,\n"
      "  \"str\": \"asdfasdf\"\n"
      "}\n"
      "{\n"
      "  \"foo1\": [1, 2, 3],\n"
      "  \"bar1\": {\"x\": 0, \"y\": null},\n"
      "  \"baz1\": true,\n"
      "  \"str1\": \"asdfasdf\"\n"
      "}\n"
    ));
    EXPECT_EQ(2, v.size());
    EXPECT_TOSTRING_EQ(
      "{"
      "\"bar\":{\"x\":0.0,\"y\":null},"
      "\"baz\":true,"
      "\"foo\":[1.0,2.0,3.0],"
      "\"str\":\"asdfasdf\""
      "}",
      v[0]);
    EXPECT_TOSTRING_EQ(
      "{"
      "\"bar1\":{\"x\":0.0,\"y\":null},"
      "\"baz1\":true,"
      "\"foo1\":[1.0,2.0,3.0],"
      "\"str1\":\"asdfasdf\""
      "}",
      v[1]);
  });
}

TEST(StreamingTest, ParseStreamReturnsByteCount) {
  GuardJsonException([](){
    json::value v;
    json::Parser p([&v](json::value r) { v = std::move(r); });
    size_t n = p.ParseStream(std::istringstream("[1]"));
    EXPECT_TOSTRING_EQ("[1.0]", v);
    EXPECT_EQ(3, n);
  });
}

TEST(StreamingTest, ParseStreamNeedsEofToParseNumbers) {
  GuardJsonException([](){
    json::value v;
    json::Parser p([&v](json::value r) { v = std::move(r); });
    size_t n = p.ParseStream(std::istringstream("123"));
    EXPECT_EQ(3, n);
    EXPECT_EQ(nullptr, v);
    // The number is not known until EOF is reached.
    p.NotifyEOF();
    EXPECT_TOSTRING_EQ("123.0", v);
  });
}

TEST(StreamingTest, ParseStreamThrowsOnParseErrorMidStream) {
  GuardJsonException([](){
    json::value v;
    json::Parser p([&v](json::value r) { v = std::move(r); });
    EXPECT_THROW(p.ParseStream(std::istringstream("400 xyz")), json::Exception);
    EXPECT_TOSTRING_EQ("400.0", v);
  });
}

TEST(StreamingTest, ParseStreamWaitsForEofOnIncompleteStream) {
  GuardJsonException([](){
    json::value v;
    json::Parser p([&v](json::value r) {
      std::cerr << "Callback invoked with " << *r << std::endl;
      v = std::move(r);
    });
    EXPECT_EQ(1, p.ParseStream(std::istringstream("[")));
    EXPECT_EQ(nullptr, v);
    // Not known to be incomplete until EOF is reached.
    EXPECT_THROW(p.NotifyEOF(), json::Exception);
  });
}

} // namespace
