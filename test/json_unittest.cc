#include "../src/json.h"
#include "gtest/gtest.h"

#include <functional>

#define EXPECT_TOSTRING_EQ(s, v) EXPECT_EQ(s, v->ToString())

namespace {

void GuardJsonException(std::function<void()> test) {
  try {
    test();
  } catch (const json::Exception& exc) {
    FAIL() << exc.what();
  }
}


// Trival null.

TEST(TrivialParseTest, Null) {
  GuardJsonException([](){
    json::value v = json::Parser::FromString("null");
    EXPECT_EQ(json::NullType, v->type());
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
    EXPECT_EQ(true, v->As<json::Boolean>()->value());
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
    EXPECT_EQ(false, v->As<json::Boolean>()->value());
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
    EXPECT_EQ(2.0, v->As<json::Number>()->value());
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
    EXPECT_EQ("", v->As<json::String>()->value());
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
    const auto& str = v->As<json::String>();
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
    EXPECT_TRUE(v->As<json::Array>()->empty());
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
    const auto& arr = v->As<json::Array>();
    EXPECT_EQ(1, arr->size());
    EXPECT_EQ(2.0, (*arr)[0]->As<json::Number>()->value());
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
    EXPECT_TRUE(v->As<json::Object>()->empty());
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
    const auto& obj = v->As<json::Object>();
    EXPECT_EQ(1, obj->size());
    EXPECT_TRUE(v->As<json::Object>()->Has("f"));
    EXPECT_FALSE(v->As<json::Object>()->Has("g"));
    EXPECT_EQ(2.0, obj->Get<json::Number>("f"));
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

TEST(BigTest, RealisticParsing) {
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

TEST(BigTest, LotsOfArrayNesting) {
  GuardJsonException([](){
    json::value v = json::Parser::FromString(
      "[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[["
      "]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]");
    EXPECT_TOSTRING_EQ(
      "[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[["
      "]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]",
      v);
  });
}

TEST(BigTest, LotsOfObjectNesting) {
  GuardJsonException([](){
    json::value v = json::Parser::FromString(
      "{\"f\":{\"f\":{\"f\":{\"f\":{\"f\":{\"f\":{\"f\":{\"f\":{\"f\":{\"f\":"
      "{\"f\":{\"f\":{\"f\":{\"f\":{\"f\":{\"f\":{\"f\":{\"f\":{\"f\":{\"f\":"
      "{\"f\":{\"f\":{\"f\":{\"f\":{\"f\":{\"f\":{\"f\":{\"f\":{\"f\":{\"f\":"
      "{\"f\":{\"f\":{\"f\":{\"f\":{\"f\":{\"f\":{\"f\":{\"f\":null"
      "}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}");
    EXPECT_TOSTRING_EQ(
      "{\"f\":{\"f\":{\"f\":{\"f\":{\"f\":{\"f\":{\"f\":{\"f\":{\"f\":{\"f\":"
      "{\"f\":{\"f\":{\"f\":{\"f\":{\"f\":{\"f\":{\"f\":{\"f\":{\"f\":{\"f\":"
      "{\"f\":{\"f\":{\"f\":{\"f\":{\"f\":{\"f\":{\"f\":{\"f\":{\"f\":{\"f\":"
      "{\"f\":{\"f\":{\"f\":{\"f\":{\"f\":{\"f\":{\"f\":{\"f\":null"
      "}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}",
      v);
  });
}


// Parse errors.

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

// Streaming parsing test.

TEST(StreamingTest, CompleteStream) {
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

TEST(StreamingTest, SplitStream) {
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

TEST(StreamingTest, BrokenStream) {
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

TEST(StreamingTest, MultipleObjectsStream) {
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
    size_t n = p.ParseStream(std::istringstream("123"));
    EXPECT_TOSTRING_EQ("123", v);
    EXPECT_EQ(3, n);
  });
}

} // namespace
