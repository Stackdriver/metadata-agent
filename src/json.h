#ifndef JSON_H_
#define JSON_H_

#include <istream>
#include <ostream>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "logging.h"

namespace json {

class Value;
using value = std::unique_ptr<Value>;

class JSONSerializer;
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

namespace {

template<class T> struct TypeHelper {};
template<> struct TypeHelper<Null> {
  static constexpr Type type = NullType;
  static constexpr char name[] = "null";
};
template<> struct TypeHelper<Boolean> {
  static constexpr Type type = BooleanType;
  static constexpr char name[] = "boolean";
};
template<> struct TypeHelper<Number> {
  static constexpr Type type = NumberType;
  static constexpr char name[] = "number";
};
template<> struct TypeHelper<String> {
  static constexpr Type type = StringType;
  static constexpr char name[] = "string";
};
template<> struct TypeHelper<Array> {
  static constexpr Type type = ArrayType;
  static constexpr char name[] = "array";
};
template<> struct TypeHelper<Object> {
  static constexpr Type type = ObjectType;
  static constexpr char name[] = "object";
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
    return type() == TypeHelper<T>::type;
  }

  // Downcast. Can be instantiated with any of the types above.
  template<class T>
  const T* As() const {
    return Is<T>() ? (T*)this : nullptr;
  }

 protected:
  friend std::ostream& operator<<(std::ostream&, const Value&);
  friend class Array;   // To call Serialize().
  friend class Object;  // To call Serialize().

  Value() = default;
  Value(const Value&) = default;

  virtual void Serialize(JSONSerializer*) const = 0;
};

class Null : public Value {
 public:
  Null() = default;
  Null(const Null&) = default;

  Type type() const override { return NullType; }

 protected:
  void Serialize(JSONSerializer*) const override;
  std::unique_ptr<Value> Clone() const override;
};

class Boolean : public Value {
 public:
  using value_type = bool;

  Boolean(bool value) : value_(value) {}
  Boolean(const Boolean&) = default;

  Type type() const override { return BooleanType; }

  bool value() const { return value_; }

 protected:
  void Serialize(JSONSerializer*) const override;
  std::unique_ptr<Value> Clone() const override;

 private:
  bool value_;
};

class Number : public Value {
 public:
  using value_type = double;

  Number(double value) : value_(value) {}
  Number(const Number&) = default;

  Type type() const override { return NumberType; }

  double value() const { return value_; }

 protected:
  void Serialize(JSONSerializer*) const override;
  std::unique_ptr<Value> Clone() const override;

 private:
  double value_;
};

class String : public Value {
 public:
  using value_type = const std::string&;

  String(const std::string& value) : value_(value) {}
  String(const String&) = default;

  Type type() const override { return StringType; }

  const std::string& value() const { return value_; }

 protected:
  void Serialize(JSONSerializer*) const override;
  std::unique_ptr<Value> Clone() const override;

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

 protected:
  void Serialize(JSONSerializer*) const override;
  std::unique_ptr<Value> Clone() const override;
};

class Object : public Value, public std::map<std::string, std::unique_ptr<Value>> {
 public:
  Object() {}
  Object(std::map<std::string, std::unique_ptr<Value>>&& fields);
  Object(std::initializer_list<std::pair<std::string, rref_capture<std::unique_ptr<Value>>>> fields);
  Object(const Object& other);

  Type type() const override { return ObjectType; }

#if 0
  // Sigh. This would have worked if we had exceptions. But I'm not willing to
  // go there... Yet...
  // Scalar field accessors.
  template<
      class T,
      class R = typename std::remove_reference<typename T::value_type>::type>
  R Get(const std::string& field) const {
    auto value_it = find(field);
    if (value_it == end()) {
      LOG(ERROR) << "There is no " << field << " in " << *this;
      return R();
    }
    if (value_it->second->type() != TypeHelper<T>::type) {
      LOG(ERROR) << field << " " << *value_it->second
                 << " is not a " << TypeHelper<T>::name;
      return R();
    }
    return value_it->second->As<T>()->value();
  }
#endif

 protected:
  void Serialize(JSONSerializer*) const override;
  std::unique_ptr<Value> Clone() const override;
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

class JSONParser {
 public:
  static std::unique_ptr<Value> FromStream(std::istream& stream);
  static std::unique_ptr<Value> FromString(const std::string& input);
};

}

#endif  // JSON_H_
