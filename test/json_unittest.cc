#include <functional>

#include "../src/json.h"
#include "gtest/gtest.h"

#define EXPECT_TO_STRING(v, s) EXPECT_EQ(v->ToString(), s)

namespace {

void GuardJsonException(std::function<void()> test) {
  try {
    test();
  } catch (const json::Exception& exc) {
    FAIL() << exc.what();
  }
}


// Trival Null

TEST(TrivialParseTest, Null) {
  GuardJsonException([](){
    std::unique_ptr<json::Value> v = json::Parser::FromString("null");
    EXPECT_EQ(v->type(), json::NullType);
  });
}

TEST(TrivialToStringTest, Null) {
  GuardJsonException([](){
    EXPECT_TO_STRING(json::null(), "null");
  });
}


// Trival True & False

TEST(TrivialParseTest, True) {
  GuardJsonException([](){
    std::unique_ptr<json::Value> v = json::Parser::FromString("true");
    EXPECT_EQ(v->As<json::Boolean>()->value(), true);
  });
}

TEST(TrivialToStringTest, True) {
  GuardJsonException([](){
    EXPECT_TO_STRING(json::boolean(true), "true");
  });
}

TEST(TrivialParseTest, False) {
  GuardJsonException([](){
    std::unique_ptr<json::Value> v = json::Parser::FromString("false");
    EXPECT_EQ(v->As<json::Boolean>()->value(), false);
  });
}

TEST(TrivialToStringTest, False) {
  GuardJsonException([](){
    EXPECT_TO_STRING(json::boolean(false), "false");
  });
}


// Trival Number

TEST(TrivialParseTest, Number) {
  GuardJsonException([](){
    std::unique_ptr<json::Value> v = json::Parser::FromString("2");
    EXPECT_EQ(v->As<json::Number>()->value(), 2.0);
  });
}

TEST(TrivialToStringTest, Number) {
  GuardJsonException([](){
    EXPECT_TO_STRING(json::number(2.0), "2.0");
  });
}


// Trival String

TEST(TrivialParseTest, StringEmpty) {
  GuardJsonException([](){
    std::unique_ptr<json::Value> v = json::Parser::FromString("\"\"");
    EXPECT_EQ(v->As<json::String>()->value().length(), 0);
  });
}

TEST(TrivialToStringTest, StringEmpty) {
  GuardJsonException([](){
    EXPECT_TO_STRING(json::string(""), "\"\"");
  });
}

TEST(TrivialParseTest, StringOneChar) {
  GuardJsonException([](){
    std::unique_ptr<json::Value> v = json::Parser::FromString("\"x\"");
    const auto& str = v->As<json::String>();
    EXPECT_EQ(str->value(), "x");
  });
}

TEST(TrivialToStringTest, StringOneChar) {
  GuardJsonException([](){
    EXPECT_TO_STRING(json::string("x"), "\"x\"");
  });
}


// Trival Array

TEST(TrivialParseTest, ArrayEmpty) {
  GuardJsonException([](){
    std::unique_ptr<json::Value> v = json::Parser::FromString("[]");
    EXPECT_TRUE(v->As<json::Array>()->empty());
  });
}

TEST(TrivialToStringTest, ArrayEmpty) {
  GuardJsonException([](){
    EXPECT_TO_STRING(json::array({}), "[]");
  });
}

TEST(TrivialParseTest, ArrayOneElement) {
  GuardJsonException([](){
    std::unique_ptr<json::Value> v = json::Parser::FromString("[2]");
    const auto& arr = v->As<json::Array>();
    EXPECT_EQ(arr->size(), 1);
    EXPECT_EQ((*arr)[0]->As<json::Number>()->value(), 2.0);
  });
}

TEST(TrivialToStringTest, ArrayOneElement) {
  GuardJsonException([](){
    EXPECT_TO_STRING(json::array({json::number(2.0)}), "[2.0]");
  });
}


// Trival Dict

TEST(TrivialParseTest, DictEmpty) {
  GuardJsonException([](){
    std::unique_ptr<json::Value> v = json::Parser::FromString("{}");
    EXPECT_TRUE(v->As<json::Object>()->empty());
  });
}

TEST(TrivialToStringTest, DictEmpty) {
  GuardJsonException([](){
    EXPECT_TO_STRING(json::object({}), "{}");
  });
}

TEST(TrivialParseTest, DictOneField) {
  GuardJsonException([](){
    std::unique_ptr<json::Value> v = json::Parser::FromString("{\"f\":2}");
    const auto& obj = v->As<json::Object>();
    EXPECT_EQ(obj->size(), 1);
    EXPECT_EQ(obj->Get<json::Number>("f"), 2.0);
  });
}

TEST(TrivialToStringTest, DictOneField) {
  GuardJsonException([](){
    std::unique_ptr<json::Value> v = json::object({
      {"f", json::number(2.0)}
    });
    EXPECT_TO_STRING(v, "{\"f\":2.0}");
  });
}


// String edge cases

/*
 * TODO(igorp): Seems that yajl does not support valid JSON.
TEST(EdgeTest, StringWithNewLine) {
  GuardJsonException([](){
    std::unique_ptr<json::Value> v = json::Parser::FromString("\"\n\"");
    EXPECT_EQ(v->As<json::String>()->value(), "\n");
    // Output should be escaped with \n as that is more canonical.
    EXPECT_TO_STRING(v, "\"\n\"");
  });
}
*/

TEST(EdgeTest, StringWithNonUnicodeEscape) {
  GuardJsonException([](){
    std::unique_ptr<json::Value> v = json::Parser::FromString("\"foo\\nbar\"");
    EXPECT_EQ(v->As<json::String>()->value(), "foo\nbar");
    // Output should be escaped as that is more canonical.
    EXPECT_TO_STRING(v, "\"foo\\nbar\"");
  });
}

TEST(EdgeTest, StringWithEveryEscape) {
  GuardJsonException([](){
    std::unique_ptr<json::Value> v = json::Parser::FromString("\"x\\\\x\\/x\\bx\\fx\\nx\\rx\\tx\"");
    EXPECT_EQ(v->As<json::String>()->value(), "x\\x/x\bx\fx\nx\rx\tx");
    // Output should be escaped as that is more canonical.
    // Since / does not need to be escaped, output does not escape that.
    EXPECT_TO_STRING(v, "\"x\\\\x/x\\bx\\fx\\nx\\rx\\tx\"");
  });
}

TEST(EdgeTest, StringWithUnicodeEscape) {
  GuardJsonException([](){
    std::unique_ptr<json::Value> v = json::Parser::FromString("\"foo\\u000abar\"");
    //EXPECT_EQ(v->As<json::String>()->value(), "foo\nbar");
    // Output should be escaped as that is more canonical.
    EXPECT_TO_STRING(v, "\"foo\\nbar\"");
  });
}

TEST(EdgeTest, StringWithUTF8) {
  GuardJsonException([](){
    // This is Korean for "Hello World!".
    std::unique_ptr<json::Value> v = json::Parser::FromString("\"안녕 세상아!\"");
    EXPECT_EQ(v->As<json::String>()->value(), "안녕 세상아!");
    EXPECT_TO_STRING(v, "\"안녕 세상아!\"");
  });
}


// Number edge cases

TEST(EdgeTest, PositiveNumbers) {
  GuardJsonException([](){
    std::unique_ptr<json::Value> v = json::Parser::FromString(
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
    EXPECT_EQ(numbers, std::vector<double>({
      0, 0e+0, 0e-0, 0e0, 0E+0, 0E-0, 0E0,
      0.0, 0.0e+0, 0.0e-0, 0.0e0, 0.0E+0, 0.0E-0, 0.0E0,
      10, 10e+0, 10e-0, 10e0, 10E+0, 10E-0, 10E0,
      10.0, 10.0e+0, 10.0e-0, 10.0e0, 10.0E+0, 10.0E-0, 10.0E0,
      123, 123.456, 789, 1.05, 1.999e-99,
    }));
    EXPECT_TO_STRING(v,
      "["
        "0.0,0.0,0.0,0.0,0.0,0.0,0.0,"
        "0.0,0.0,0.0,0.0,0.0,0.0,0.0,"
        "10.0,10.0,10.0,10.0,10.0,10.0,10.0,"
        "10.0,10.0,10.0,10.0,10.0,10.0,10.0,"
        "123.0,123.45600000000000307,789.0,"
        "1.0500000000000000444,1.9989999999999998999e-99"
      "]"
    );
  });
}

TEST(EdgeTest, NegativeNumbers) {
  GuardJsonException([](){
    std::unique_ptr<json::Value> v = json::Parser::FromString(
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
    EXPECT_EQ(numbers, std::vector<double>({
      -0, -0e+0, -0e-0, -0e0, -0E+0, -0E-0, -0E0,
      -0.0, -0.0e+0, -0.0e-0, -0.0e0, -0.0E+0, -0.0E-0, -0.0E0,
      -10, -10e+0, -10e-0, -10e0, -10E+0, -10E-0, -10E0,
      -10.0, -10.0e+0, -10.0e-0, -10.0e0, -10.0E+0, -10.0E-0, -10.0E0,
      -123, -123.456, -789, -1.05, -1.999e-99,
    }));
    // TODO(igorp): This first one looks spurious.
    EXPECT_TO_STRING(v,
      "["
        "0.0,-0.0,-0.0,-0.0,-0.0,-0.0,-0.0,"
        "-0.0,-0.0,-0.0,-0.0,-0.0,-0.0,-0.0,"
        "-10.0,-10.0,-10.0,-10.0,-10.0,-10.0,-10.0,"
        "-10.0,-10.0,-10.0,-10.0,-10.0,-10.0,-10.0,"
        "-123.0,-123.45600000000000307,-789.0,"
        "-1.0500000000000000444,-1.9989999999999998999e-99"
      "]"
    );
  });
}


// Big tests.

TEST(BigTest, Realistic) {
  GuardJsonException([](){
    std::unique_ptr<json::Value> v = json::Parser::FromString(
      "{\n"
      "  \"foo\": [1, 2, 3],\n"
      "  \"bar\": {\"x\": 0, \"y\": null},\n"
      "  \"baz\": true,\n"
      "  \"str\": \"asdfasdf\"\n"
      "}\n"
    );
    EXPECT_TO_STRING(v,
      "{"
      "\"bar\":{\"x\":0.0,\"y\":null},"
      "\"baz\":true,"
      "\"foo\":[1.0,2.0,3.0],"
      "\"str\":\"asdfasdf\""
      "}"
    );
  });
}

TEST(BigTest, LotsOfNesting) {
  GuardJsonException([](){
    std::unique_ptr<json::Value> v = json::Parser::FromString(
      "[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[["
      "]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]"
    );
    EXPECT_TO_STRING(v,
      "[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[["
      "]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]"
    );
  });
}


// Parse errors

TEST(ParseError, Empty) {
  ASSERT_THROW(json::Parser::FromString(""), json::Exception);
}

TEST(ParseError, UnexpectedChar) {
  ASSERT_THROW(json::Parser::FromString("x"), json::Exception);
}

TEST(ParseError, UnterminatedArray) {
  ASSERT_THROW(json::Parser::FromString("["), json::Exception);
}

TEST(ParseError, UnmatchedArrayClose) {
  ASSERT_THROW(json::Parser::FromString("]"), json::Exception);
}

TEST(ParseError, UnterminatedObject) {
  ASSERT_THROW(json::Parser::FromString("{"), json::Exception);
}

TEST(ParseError, UnmatchedObjectClose) {
  ASSERT_THROW(json::Parser::FromString("}"), json::Exception);
}

TEST(ParseError, UnterminatedString) {
  ASSERT_THROW(json::Parser::FromString("\""), json::Exception);
}

TEST(ParseError, UnterminatedEscape) {
  ASSERT_THROW(json::Parser::FromString("\"\\\""), json::Exception);
}

TEST(ParseError, UnterminatedUnicodeEscape) {
  ASSERT_THROW(json::Parser::FromString("\"\\u\""), json::Exception);
  ASSERT_THROW(json::Parser::FromString("\"\\u0\""), json::Exception);
  ASSERT_THROW(json::Parser::FromString("\"\\u00\""), json::Exception);
  ASSERT_THROW(json::Parser::FromString("\"\\u000\""), json::Exception);
}

TEST(ParseError, ObjectNoValue) {
  ASSERT_THROW(json::Parser::FromString("{\"x\"}"), json::Exception);
  ASSERT_THROW(json::Parser::FromString("{\"x\":}"), json::Exception);
}


} // namespace
