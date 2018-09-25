/*
 * Copyright 2017 Google Inc.
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

#include "json.h"

#include <cmath>
#include <iostream>
#include <iterator>
#include <limits>
#include <sstream>
#include <utility>

#include <yajl/yajl_gen.h>
#include <yajl/yajl_parse.h>
//#if HAVE_YAJL_YAJL_VERSION_H
//# include <yajl/yajl_version.h>
//#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace json {

namespace internal {

constexpr const char TypeHelper<Null>::name[];
constexpr const char TypeHelper<Boolean>::name[];
constexpr const char TypeHelper<Number>::name[];
constexpr const char TypeHelper<String>::name[];
constexpr const char TypeHelper<Array>::name[];
constexpr const char TypeHelper<Object>::name[];

struct JSONSerializer {
  JSONSerializer();
  ~JSONSerializer();

  std::string ToString();
  void ToStream(std::ostream& stream);

  yajl_gen& gen() { return gen_; }

 private:
  std::pair<const unsigned char*, size_t> buf();
  void clear();
  yajl_gen gen_;
};

JSONSerializer::JSONSerializer() {
  gen_ = yajl_gen_alloc(NULL);
  //yajl_gen_config(gen, yajl_gen_beautify, 1);
  yajl_gen_config(gen_, yajl_gen_validate_utf8, 1);
}

JSONSerializer::~JSONSerializer() {
  yajl_gen_free(gen_);
}

std::pair<const unsigned char*, size_t> JSONSerializer::buf() {
  size_t len;
  const unsigned char* buf;
  yajl_gen_get_buf(gen_, &buf, &len);
  return {buf, len};
}

void JSONSerializer::clear() {
  yajl_gen_clear(gen_);
}

void JSONSerializer::ToStream(std::ostream& stream) {
  const auto buf_and_len = buf();
  stream.write(reinterpret_cast<const char*>(buf_and_len.first),
               buf_and_len.second);
}

std::string JSONSerializer::ToString() {
  const auto buf_and_len = buf();
  return {reinterpret_cast<const char*>(buf_and_len.first), buf_and_len.second};
}

}  // internal

std::string Value::ToString() const {
  internal::JSONSerializer serializer;
  Serialize(&serializer);
  return serializer.ToString();
}

std::ostream& operator<<(std::ostream& stream, const Value& value) {
  internal::JSONSerializer serializer;
  value.Serialize(&serializer);
  serializer.ToStream(stream);
  return stream;
}

void Null::Serialize(internal::JSONSerializer* serializer) const {
  yajl_gen_null(serializer->gen());
}

std::unique_ptr<Value> Null::Clone() const {
  return std::unique_ptr<Value>(new Null());
}

void Boolean::Serialize(internal::JSONSerializer* serializer) const {
  yajl_gen_bool(serializer->gen(), value_);
}

std::unique_ptr<Value> Boolean::Clone() const {
  return std::unique_ptr<Value>(new Boolean(value_));
}

namespace {
template<class IntType>
constexpr bool IsEffectivelyInteger(long double value) {
  return (value == std::floor(value) &&
          value >= std::numeric_limits<IntType>::min() &&
          value <= std::numeric_limits<IntType>::max() &&
          !(value == 0.0 && std::signbit(value)));
}
}

void Number::Serialize(internal::JSONSerializer* serializer) const {
  if (IsEffectivelyInteger<long long>(value_)) {
    yajl_gen_integer(serializer->gen(), static_cast<long long>(value_));
  } else {
    yajl_gen_double(serializer->gen(), static_cast<double>(value_));
  }
}

std::unique_ptr<Value> Number::Clone() const {
  return std::unique_ptr<Value>(new Number(value_));
}

void String::Serialize(internal::JSONSerializer* serializer) const {
  yajl_gen_string(serializer->gen(),
                  reinterpret_cast<const unsigned char*>(value_.data()),
                  value_.size());
}

std::unique_ptr<Value> String::Clone() const {
  return std::unique_ptr<Value>(new String(value_));
}

Array::Array(const Array& other) {
  for (const auto& e : other) {
    emplace_back(e->Clone());
  }
}

Array::Array(std::vector<std::unique_ptr<Value>>&& elements) {
  for (auto& e : elements) {
    if (e != nullptr) {
      emplace_back(std::move(e));
    }
  }
}

Array::Array(
    std::initializer_list<rref_capture<std::unique_ptr<Value>>> elements) {
  for (auto& e : elements) {
    if (((std::unique_ptr<Value>&&) e) != nullptr) {
      emplace_back(std::move(e));
    }
  }
}

void Array::Serialize(internal::JSONSerializer* serializer) const {
  yajl_gen_array_open(serializer->gen());
  for (const auto& e : *this) {
    e->Serialize(serializer);
  }
  yajl_gen_array_close(serializer->gen());
}

std::unique_ptr<Value> Array::Clone() const {
  return std::unique_ptr<Value>(new Array(*this));
}

Object::Object(const Object& other) {
  for (const auto& kv : other) {
    emplace(kv.first, kv.second->Clone());
  }
}

Object::Object(std::map<std::string, std::unique_ptr<Value>>&& fields) {
  for (auto& kv : fields) {
    if (kv.second != nullptr) {
      emplace(kv.first, std::move(kv.second));
    }
  }
}

Object::Object(
    std::initializer_list<std::pair<std::string,
                          rref_capture<std::unique_ptr<Value>>>> fields) {
  for (auto& kv : fields) {
    if (((std::unique_ptr<Value>&&) kv.second) != nullptr) {
      emplace(kv.first, std::move(kv.second));
    }
  }
}

void Object::Serialize(internal::JSONSerializer* serializer) const {
  yajl_gen_map_open(serializer->gen());
  for (const auto& e : *this) {
    yajl_gen_string(serializer->gen(),
                    reinterpret_cast<const unsigned char*>(e.first.data()),
                    e.first.size());
    e.second->Serialize(serializer);
  }
  yajl_gen_map_close(serializer->gen());
}

std::unique_ptr<Value> Object::Clone() const {
  return std::unique_ptr<Value>(new Object(*this));
}

namespace {

class Context {
 public:
  virtual ~Context() = default;
  virtual void AddValue(std::unique_ptr<Value> value) = 0;
  std::unique_ptr<Context> parent() { return std::move(parent_); }
 protected:
  Context(std::unique_ptr<Context> parent) : parent_(std::move(parent)) {}
 private:
  std::unique_ptr<Context> parent_;
};

class ArrayContext : public Context {
 public:
  ArrayContext(std::unique_ptr<Context> parent) : Context(std::move(parent)) {}
  void AddValue(std::unique_ptr<Value> value) override {
    elements.emplace_back(std::move(value));
  }
  std::vector<std::unique_ptr<Value>> elements;
};

class ObjectContext : public Context {
 public:
  ObjectContext(std::unique_ptr<Context> parent) : Context(std::move(parent)) {}
  void NewField(const std::string& name) {
    if (field_name_ != nullptr) {
      std::cerr << "Replacing " << *field_name_
                << " with " << name << std::endl;
    }
    field_name_.reset(new std::string(name));
  }
  void AddValue(std::unique_ptr<Value> value) override {
    if (field_name_ == nullptr) {
      std::cerr << "Value without a field name" << *value << std::endl;
      return;
    }
    fields.emplace(*field_name_, std::move(value));
    field_name_.reset();
  }
  std::map<std::string, std::unique_ptr<Value>> fields;
 private:
  std::unique_ptr<std::string> field_name_;
};

class CallbackContext : public Context {
 public:
  CallbackContext(std::function<void(std::unique_ptr<Value>)> callback)
      : Context(nullptr), callback_(callback) {}
  void AddValue(std::unique_ptr<Value> value) override {
    callback_(std::move(value));
  }
 private:
  std::function<void(std::unique_ptr<Value>)> callback_;
};

// A builder context that allows building up a JSON object.
class JSONBuilder {
 public:
  JSONBuilder(std::function<void(std::unique_ptr<Value>)> callback)
      : context_(new CallbackContext(callback)) {}
  ~JSONBuilder() = default;

  void AddValue(std::unique_ptr<Value> value) {
    context_->AddValue(std::move(value));
  }

  void PushArray() {
    context_.reset(new ArrayContext(std::move(context_)));
  }

  void PushObject() {
    context_.reset(new ObjectContext(std::move(context_)));
  }

  std::unique_ptr<ArrayContext> PopArray() {
    ArrayContext* array_context =
        dynamic_cast<ArrayContext*>(context_.get());
    if (array_context == nullptr) {
      std::cerr << "Not in array context" << std::endl;
      return nullptr;
    }
    context_ = context_.release()->parent();
    return std::unique_ptr<ArrayContext>(array_context);
  }

  std::unique_ptr<ObjectContext> PopObject() {
    ObjectContext* object_context =
        dynamic_cast<ObjectContext*>(context_.get());
    if (object_context == nullptr) {
      std::cerr << "Not in object context" << std::endl;
      return nullptr;
    }
    context_ = context_.release()->parent();
    return std::unique_ptr<ObjectContext>(object_context);
  }

  // Objects only.
  bool NewField(const std::string& name) {
    ObjectContext* object_context =
        dynamic_cast<ObjectContext*>(context_.get());
    if (object_context == nullptr) {
      std::cerr << "NewField " << name << " outside of object" << std::endl;
      return false;
    }
    object_context->NewField(name);
    return true;
  }

 private:
  std::unique_ptr<Context> context_;
};

int handle_null(void* arg) {
  JSONBuilder* builder = reinterpret_cast<JSONBuilder*>(arg);
  builder->AddValue(std::unique_ptr<Value>(new Null()));
  return 1;
}

int handle_bool(void* arg, int value) {
  JSONBuilder* builder = reinterpret_cast<JSONBuilder*>(arg);
  builder->AddValue(
      std::unique_ptr<Value>(new Boolean(static_cast<bool>(value))));
  return 1;
}

int handle_string(void* arg, const unsigned char* val, size_t length) {
  std::string value(reinterpret_cast<const char*>(val), length);
  JSONBuilder* builder = reinterpret_cast<JSONBuilder*>(arg);
  builder->AddValue(std::unique_ptr<Value>(new String(value)));
  return 1;
}

int handle_integer(void* arg, long long value) {
  JSONBuilder* builder = reinterpret_cast<JSONBuilder*>(arg);
  builder->AddValue(
      std::unique_ptr<Value>(new Number(value)));
  return 1;
}

int handle_double(void* arg, double value) {
  JSONBuilder* builder = reinterpret_cast<JSONBuilder*>(arg);
  builder->AddValue(std::unique_ptr<Value>(new Number(value)));
  return 1;
}

int handle_start_array(void* arg) {
  JSONBuilder* builder = reinterpret_cast<JSONBuilder*>(arg);
  builder->PushArray();
  return 1;
}

static int handle_end_array(void* arg) {
  JSONBuilder* builder = reinterpret_cast<JSONBuilder*>(arg);
  std::unique_ptr<ArrayContext> array_context = builder->PopArray();
  if (array_context == nullptr) {
    return 0;
  }
  builder->AddValue(
      std::unique_ptr<Value>(new Array(std::move(array_context->elements))));
  return 1;
}

int handle_start_map(void* arg) {
  JSONBuilder* builder = reinterpret_cast<JSONBuilder*>(arg);
  builder->PushObject();
  return 1;
}

int handle_map_key(void* arg, const unsigned char* data, size_t length) {
  std::string key(reinterpret_cast<const char*>(data), length);
  JSONBuilder* builder = reinterpret_cast<JSONBuilder*>(arg);
  return builder->NewField(key) ? 1 : 0;
}

int handle_end_map(void* arg) {
  JSONBuilder* builder = reinterpret_cast<JSONBuilder*>(arg);
  std::unique_ptr<ObjectContext> object_context = builder->PopObject();
  if (object_context == nullptr) {
    return 0;
  }
  builder->AddValue(
      std::unique_ptr<Value>(new Object(std::move(object_context->fields))));
  return 1;
}

const yajl_callbacks callbacks = {
    .yajl_null = &handle_null,
    .yajl_boolean = &handle_bool,
    .yajl_integer = &handle_integer,
    .yajl_double = &handle_double,
    .yajl_number = nullptr,
    .yajl_string = &handle_string,
    .yajl_start_map = &handle_start_map,
    .yajl_map_key = &handle_map_key,
    .yajl_end_map = &handle_end_map,
    .yajl_start_array = &handle_start_array,
    .yajl_end_array = &handle_end_array,
};

class YajlHandle {
 public:
  YajlHandle(JSONBuilder* builder)
      : handle_(yajl_alloc(&callbacks, NULL, builder)) {
    yajl_config(handle_, yajl_allow_comments, 1);
    yajl_config(handle_, yajl_allow_multiple_values, 1);
    //yajl_config(handle_, yajl_allow_partial_values, 1);
    //yajl_config(handle_, yajl_allow_trailing_garbage, 1);
    //yajl_config(handle_, yajl_dont_validate_strings, 1);
  }
  ~YajlHandle() {
    yajl_free(handle_);
  }
  operator yajl_handle() { return handle_; }
 private:
  yajl_handle handle_;
};

class YajlError {
 public:
  YajlError(yajl_handle handle, bool verbose,
            const unsigned char* json_text, size_t json_len)
      : handle_(handle),
        str_(yajl_get_error(handle_, (int)verbose, json_text, json_len)) {}
  ~YajlError() {
    yajl_free_error(handle_, str_);
  }
  const char* c_str() { return reinterpret_cast<const char*>(str_); }
 private:
  yajl_handle handle_;
  unsigned char* str_;
};

}  // namespace

std::vector<std::unique_ptr<Value>> Parser::AllFromStream(std::istream& stream)
    throw(Exception)
{
  std::vector<std::unique_ptr<Value>> values;
  Parser p([&values](std::unique_ptr<Value> r){
    values.emplace_back(std::move(r));
  });
  p.ParseStream(stream);
  p.NotifyEOF();
  return values;
}

std::unique_ptr<Value> Parser::FromStream(std::istream& stream)
    throw(Exception)
{
  std::vector<std::unique_ptr<Value>> all_values = AllFromStream(stream);
  if (all_values.empty()) {
    return nullptr;
  }
  if (all_values.size() > 1) {
    std::ostringstream out;
    out << "Single value expected, " << all_values.size() << " values seen";
    throw Exception(out.str());
  }
  return std::move(all_values[0]);
}

std::vector<std::unique_ptr<Value>> Parser::AllFromString(
    const std::string& input) throw(Exception)
{
  return AllFromStream(std::istringstream(input));
}

std::unique_ptr<Value> Parser::FromString(const std::string& input)
    throw(Exception)
{
  return FromStream(std::istringstream(input));
}

class Parser::ParseState {
 public:
  ParseState(std::function<void(std::unique_ptr<Value>)> callback)
      : builder_(callback), handle_(&builder_) {
    yajl_config(handle_, yajl_allow_comments, 1);
    yajl_config(handle_, yajl_allow_multiple_values, 1);
    //yajl_config(handle_, yajl_allow_partial_values, 1);
    //yajl_config(handle_, yajl_allow_trailing_garbage, 1);
    //yajl_config(handle_, yajl_dont_validate_strings, 1);
  }

  void Done() throw(Exception) {
    yajl_status stat = yajl_complete_parse(handle_);
    if (stat != yajl_status_ok) {
      YajlError err(handle_, 0, nullptr, 0);
      throw Exception(err.c_str());
    }
  }

  yajl_handle handle() { return handle_; }

 private:
  JSONBuilder builder_;
  YajlHandle handle_;
};

Parser::Parser(std::function<void(std::unique_ptr<Value>)> callback)
    : state_(new ParseState(callback)) {}

Parser::~Parser() = default;

std::size_t Parser::ParseStream(std::istream& stream) throw(Exception) {
  const int kMax = 65536;
  unsigned char data[kMax] = {0};
  size_t total_bytes_consumed = 0;
  yajl_handle handle = state_->handle();

  while (!stream.eof()) {
    stream.read(reinterpret_cast<char*>(&data[0]), kMax);
    size_t count = stream.gcount();

    yajl_status stat = yajl_parse(handle, data, count);
    if (stat != yajl_status_ok) {
      YajlError err(handle, 1, data, kMax);
      throw Exception(err.c_str());
    }

    total_bytes_consumed += yajl_get_bytes_consumed(handle);
  }

  return total_bytes_consumed;
}

void Parser::NotifyEOF() throw(Exception) {
  state_->Done();
}

}  // json
