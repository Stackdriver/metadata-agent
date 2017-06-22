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

//#include "config.h"

#include "json.h"

#include <iostream>
#include <iterator>
#include <sstream>
#include <utility>

#include <yajl/yajl_gen.h>
#include <yajl/yajl_parse.h>
//#if HAVE_YAJL_YAJL_VERSION_H
//# include <yajl/yajl_version.h>
//#endif

#include <yajl/yajl_parse.h>
#include <yajl/yajl_gen.h>

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
  const auto buf_len = buf();
  stream.write(reinterpret_cast<const char*>(buf_len.first), buf_len.second);
}

std::string JSONSerializer::ToString() {
  const auto buf_len = buf();
  return {reinterpret_cast<const char*>(buf_len.first), buf_len.second};
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

void Number::Serialize(internal::JSONSerializer* serializer) const {
  yajl_gen_double(serializer->gen(), value_);
}

std::unique_ptr<Value> Number::Clone() const {
  return std::unique_ptr<Value>(new Number(value_));
}

void String::Serialize(internal::JSONSerializer* serializer) const {
  yajl_gen_string(serializer->gen(), reinterpret_cast<const unsigned char*>(value_.data()), value_.size());
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
    emplace_back(std::move(e));
  }
}

Array::Array(std::initializer_list<rref_capture<std::unique_ptr<Value>>> elements) {
  for (auto& e : elements) {
    emplace_back(std::move(e));
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
    emplace(kv.first, std::move(kv.second));
  }
}

Object::Object(std::initializer_list<std::pair<std::string, rref_capture<std::unique_ptr<Value>>>> fields) {
  for (auto& kv : fields) {
    emplace(kv.first, std::move(kv.second));
  }
}

void Object::Serialize(internal::JSONSerializer* serializer) const {
  yajl_gen_map_open(serializer->gen());
  for (const auto& e : *this) {
    yajl_gen_string(serializer->gen(), reinterpret_cast<const unsigned char*>(e.first.data()), e.first.size());
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
  virtual void AddValue(std::unique_ptr<Value> value) = 0;
  Context* parent() { return parent_; }
  virtual ~Context() = default;
 protected:
  Context(Context* parent) : parent_(parent) {}
 private:
  Context* parent_;
};

class TopLevelContext : public Context {
 public:
  TopLevelContext() : Context(nullptr) {}
  void AddValue(std::unique_ptr<Value> value) override {
    if (value_ != nullptr) {
      std::cerr << "Replacing " << *value_
                << " with " << *value << std::endl;
    }
    value_ = std::move(value);
  }
  std::unique_ptr<Value> value() {
    return std::move(value_);
  }
 private:
  std::unique_ptr<Value> value_;
};

class ArrayContext : public Context {
 public:
  ArrayContext(Context* parent) : Context(parent) {}
  void AddValue(std::unique_ptr<Value> value) override {
    elements.emplace_back(std::move(value));
  }
  std::vector<std::unique_ptr<Value>> elements;
};

class ObjectContext : public Context {
 public:
  ObjectContext(Context* parent) : Context(parent) {}
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

// A builder context that allows building up a JSON object.
class JSONBuilder {
 public:
  JSONBuilder() : context_(new TopLevelContext()) {}
  ~JSONBuilder() { delete context_; }

  void AddValue(std::unique_ptr<Value> value) {
    context_->AddValue(std::move(value));
  }

  void PushArray() {
    context_ = new ArrayContext(context_);
  }
  void PushObject() {
    context_ = new ObjectContext(context_);
  }

  std::unique_ptr<ArrayContext> PopArray() {
    std::unique_ptr<ArrayContext> array_context(
        dynamic_cast<ArrayContext*>(context_));
    if (array_context == nullptr) {
      std::cerr << "Not in array context" << std::endl;
      return nullptr;
    }
    context_ = context_->parent();
    return array_context;
  }
  std::unique_ptr<ObjectContext> PopObject() {
    std::unique_ptr<ObjectContext> object_context(
        dynamic_cast<ObjectContext*>(context_));
    if (object_context == nullptr) {
      std::cerr << "Not in object context" << std::endl;
      return nullptr;
    }
    context_ = context_->parent();
    return object_context;
  }

  // Objects only.
  bool NewField(const std::string& name) {
    ObjectContext* object_context = dynamic_cast<ObjectContext*>(context_);
    if (object_context == nullptr) {
      std::cerr << "NewField " << name << " outside of object" << std::endl;
      return false;
    }
    object_context->NewField(name);
    return true;
  }

  // Top-level context only.
  std::unique_ptr<Value> value() {
    TopLevelContext* top_level = dynamic_cast<TopLevelContext*>(context_);
    if (top_level == nullptr) {
      std::cerr << "value() called for an inner context" << std::endl;
      return nullptr;
    }
    return top_level->value();
  }

 private:
  Context* context_;
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
      std::unique_ptr<Value>(new Number(static_cast<double>(value))));
  return 1;
}

int handle_double(void* arg, double value) {
  JSONBuilder* builder = reinterpret_cast<JSONBuilder*>(arg);
  builder->AddValue(std::unique_ptr<Value>(new Number(value)));
  return 1;
}

int handle_number(void* arg, const char* data, size_t length) {
  return handle_string(arg, (const unsigned char*)data, length);
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

yajl_callbacks callbacks = {
    .yajl_null = &handle_null,
    .yajl_boolean = &handle_bool,
    .yajl_integer = &handle_integer,
    .yajl_double = &handle_double,
    //.yajl_number = &handle_number,
    .yajl_number = nullptr,
    .yajl_string = &handle_string,
    .yajl_start_map = &handle_start_map,
    .yajl_map_key = &handle_map_key,
    .yajl_end_map = &handle_end_map,
    .yajl_start_array = &handle_start_array,
    .yajl_end_array = &handle_end_array,
};

}

std::unique_ptr<Value> Parser::FromStream(std::istream& stream) {
  JSONBuilder builder;

  const int kMax = 65536;
  unsigned char data[kMax];
  yajl_handle handle = yajl_alloc(&callbacks, NULL, (void*) &builder);
  yajl_config(handle, yajl_allow_comments, 1);
  //yajl_config(handle, yajl_dont_validate_strings, 1);

  for (;;) {
    if (stream.eof()) {
      break;
    }
    stream.read(reinterpret_cast<char*>(&data[0]), kMax);
    size_t count = stream.gcount();
    yajl_parse(handle, data, count);
  }

  yajl_status stat = yajl_complete_parse(handle);

  if (stat != yajl_status_ok) {
    unsigned char* str = yajl_get_error(handle, 1, data, kMax);
    // TODO cerr << str << endl;
    yajl_free_error(handle, str);
    return nullptr;
  }

  yajl_free(handle);
  return builder.value();
}

std::unique_ptr<Value> Parser::FromString(const std::string& input) {
  std::stringstream stream(input);
  return FromStream(stream);
}

}  // json
