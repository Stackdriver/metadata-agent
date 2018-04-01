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
#ifndef JSON_H_
#define JSON_H_

#include <functional>
#include <istream>
#include <ostream>
#include <map>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "logging.h"

namespace json {

// A representation of all JSON-related errors.
class Exception {
 public:
  Exception(const std::string& what) : explanation_(what) {}
  const std::string& what() const { return explanation_; }
 private:
  std::string explanation_;
};

class Value;
using value = std::unique_ptr<Value>;

class Null;
class Boolean;
class Number;
class String;
class Array;
class Object;

// A JSON type. All other JSON values inherit from Value.
enum Type {
  NullType,
  BooleanType,
  NumberType,
  StringType,
  ArrayType,
  ObjectType,
};

namespace internal {

class JSONSerializer;

template<class T> struct TypeHelper {};
template<> struct TypeHelper<Null> {
  static constexpr Type type = NullType;
  static constexpr const char name[] = "null";
};
template<> struct TypeHelper<Boolean> {
  static constexpr Type type = BooleanType;
  static constexpr const char name[] = "boolean";
};
template<> struct TypeHelper<Number> {
  static constexpr Type type = NumberType;
  static constexpr const char name[] = "number";
};
template<> struct TypeHelper<String> {
  static constexpr Type type = StringType;
  static constexpr const char name[] = "string";
};
template<> struct TypeHelper<Array> {
  static constexpr Type type = ArrayType;
  static constexpr const char name[] = "array";
};
template<> struct TypeHelper<Object> {
  static constexpr Type type = ObjectType;
  static constexpr const char name[] = "object";
};

template<char F, class Enable = void>
struct ArticleHelper {
  static constexpr const char* value() { return "a"; }
};
template<char F>
struct ArticleHelper<F, typename std::enable_if<F=='a'||F=='e'||F=='i'||F=='o'||F=='u'>::type> {
  static constexpr const char* value() { return "an"; }
};

}

// A JSON value. All other JSON values inherit from Value.
class Value {
 public:
  virtual ~Value() {}

  std::string ToString() const;

  virtual Type type() const = 0;

  virtual std::unique_ptr<Value> Clone() const = 0;

  // Type check. Can be instantiated with any of the types above.
  template<class T>
  bool Is() const {
    return type() == internal::TypeHelper<T>::type;
  }

  // Downcast. Can be instantiated with any of the types above.
  template<class T>
  const T* As() const throw(Exception) {
    if (!Is<T>()) {
      constexpr const char* name = internal::TypeHelper<T>::name;
      constexpr const char* article = internal::ArticleHelper<name[0]>::value();
      throw Exception(ToString() + " is not " + article + " " + name);
    }
    return static_cast<const T*>(this);
  }

 protected:
  friend std::ostream& operator<<(std::ostream&, const Value&);
  friend class Array;   // To call Serialize().
  friend class Object;  // To call Serialize().

  Value() = default;
  Value(const Value&) = default;

  virtual void Serialize(internal::JSONSerializer*) const = 0;
};

class Null : public Value {
 public:
  Null() = default;
  Null(const Null&) = default;

  Type type() const override { return NullType; }

  std::unique_ptr<Value> Clone() const override;

 protected:
  void Serialize(internal::JSONSerializer*) const override;
};

class Boolean : public Value {
 public:
  Boolean(bool value) : value_(value) {}
  Boolean(const Boolean&) = default;

  Type type() const override { return BooleanType; }

  std::unique_ptr<Value> Clone() const override;

  bool value() const { return value_; }

 protected:
  void Serialize(internal::JSONSerializer*) const override;

 private:
  bool value_;
};

class Number : public Value {
 public:
  Number(double value) : value_(value) {}
  Number(const Number&) = default;

  Type type() const override { return NumberType; }

  std::unique_ptr<Value> Clone() const override;

  double value() const { return value_; }

 protected:
  void Serialize(internal::JSONSerializer*) const override;

 private:
  double value_;
};

class String : public Value {
 public:
  String(const std::string& value) : value_(value) {}
  String(const String&) = default;

  Type type() const override { return StringType; }

  std::unique_ptr<Value> Clone() const override;

  const std::string& value() const { return value_; }

 protected:
  void Serialize(internal::JSONSerializer*) const override;

 private:
  std::string value_;
};

// A wrapper class for unique_ptrs in initializer_lists.
// See http://stackoverflow.com/questions/8193102/.
template<typename T> class rref_capture {
 public:
  rref_capture(T&& x) : ptr_(&x) {}
  operator T&& () const { return std::move(*ptr_); }
 private:
  T* ptr_;
};

class Array : public Value, public std::vector<std::unique_ptr<Value>> {
 public:
  Array() {}
  Array(std::vector<std::unique_ptr<Value>>&& elements);
  Array(std::initializer_list<rref_capture<std::unique_ptr<Value>>> elements);
  Array(const Array& other);

  Type type() const override { return ArrayType; }

  std::unique_ptr<Value> Clone() const override;

 protected:
  void Serialize(internal::JSONSerializer*) const override;
};

class Object : public Value, public std::map<std::string, std::unique_ptr<Value>> {
 public:
  Object() {}
  Object(std::map<std::string, std::unique_ptr<Value>>&& fields);
  Object(std::initializer_list<std::pair<std::string, rref_capture<std::unique_ptr<Value>>>> fields);
  Object(const Object& other);

  Type type() const override { return ObjectType; }

  std::unique_ptr<Value> Clone() const override;

 private:
  // Field accessor common functionality.
  template<class T>
  const T* GetField(const std::string& field) const throw(Exception) {
    auto value_it = find(field);
    if (value_it == end()) {
      throw Exception("There is no " + field + " in " + ToString());
    }
    if (value_it->second->type() != internal::TypeHelper<T>::type) {
      constexpr const char* name = internal::TypeHelper<T>::name;
      constexpr const char* article = internal::ArticleHelper<name[0]>::value();
      throw Exception(field + " " + value_it->second->ToString() +
                      " is not " + article + " " + name);
    }
    return value_it->second->As<T>();
  }
  template <class T, class R>
  friend struct FieldGetter;

  template<typename T> struct Void { using type = void; };

  // Field accessors.
  template<class T, class R = void>
  struct FieldGetter {
    using return_type = const T*;
    static const T* GetField(const Object* obj, const std::string& field)
        throw(Exception)
    {
      return obj->GetField<T>(field);
    }
  };
  // Scalar field accessors (for types that define a value() method).
  template<class T>
  struct FieldGetter<T, typename Void<decltype(&T::value)>::type> {
    using value_type = typename std::result_of<decltype(&T::value)(T)>::type;
    using return_type = typename std::remove_reference<value_type>::type;
    static return_type GetField(const Object* obj, const std::string& field)
        throw(Exception)
    {
      return obj->GetField<T>(field)->value();
    }
  };

 public:
  bool Has(const std::string& field) const { return find(field) != end(); }

  // Field accessors.
  template<class T>
  typename FieldGetter<T>::return_type Get(const std::string& field) const
      throw(Exception)
  {
    return FieldGetter<T>::GetField(this, field);
  }

 protected:
  void Serialize(internal::JSONSerializer*) const override;
};

// Factory functions.
inline std::unique_ptr<Value> null() {
  return std::unique_ptr<Null>(new Null());
}
inline std::unique_ptr<Value> boolean(bool value) {
  return std::unique_ptr<Boolean>(new Boolean(value));
}
inline std::unique_ptr<Value> number(double value) {
  return std::unique_ptr<Number>(new Number(value));
}
inline std::unique_ptr<Value> string(const std::string& value) {
  return std::unique_ptr<String>(new String(value));
}
inline std::unique_ptr<Value> array(
    std::vector<std::unique_ptr<Value>>&& elements) {
  return std::unique_ptr<Array>(new Array(std::move(elements)));
}
inline std::unique_ptr<Value> array(
    std::initializer_list<rref_capture<std::unique_ptr<Value>>> elements) {
  return std::unique_ptr<Array>(new Array(elements));
}
inline std::unique_ptr<Value> object(
    std::map<std::string, std::unique_ptr<Value>>&& fields) {
  return std::unique_ptr<Object>(new Object(std::move(fields)));
}
inline std::unique_ptr<Value> object(
    std::initializer_list<std::pair<std::string, rref_capture<std::unique_ptr<Value>>>> fields) {
  return std::unique_ptr<Object>(new Object(fields));
}

class Parser {
 public:
  class ParseState;

  Parser(std::function<void(std::unique_ptr<Value>)> callback);
  ~Parser();
  static std::vector<std::unique_ptr<Value>> AllFromStream(
      std::istream& stream) throw(Exception);
  static std::vector<std::unique_ptr<Value>> AllFromString(
      const std::string& input) throw(Exception);
  static std::unique_ptr<Value> FromStream(std::istream& stream)
      throw(Exception);
  static std::unique_ptr<Value> FromString(const std::string& input)
      throw(Exception);

  size_t ParseStream(std::istream& stream) throw(Exception);
  // Used to accept inline construction of streams.
  size_t ParseStream(std::istream&& stream) throw(Exception) {
    return ParseStream(stream);
  }
  // Notifies the parser that no more data is available.
  void NotifyEOF() throw(Exception);

 private:
  std::unique_ptr<ParseState> state_;
};

}

#endif  // JSON_H_
