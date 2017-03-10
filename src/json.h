#include <istream>
#include <ostream>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace json {

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

template<class T> struct Type_ {};
template<> struct Type_<Null> { static constexpr Type type = NullType; };
template<> struct Type_<Boolean> { static constexpr Type type = BooleanType; };
template<> struct Type_<Number> { static constexpr Type type = NumberType; };
template<> struct Type_<String> { static constexpr Type type = StringType; };
template<> struct Type_<Array> { static constexpr Type type = ArrayType; };
template<> struct Type_<Object> { static constexpr Type type = ObjectType; };

}

// A JSON value. All other JSON values inherit from Value.
class Value {
 public:
  virtual ~Value() {}

  std::string ToJSON() const;

  virtual Type type() const = 0;

  virtual std::unique_ptr<Value> Clone() const = 0;

  // Downcast. Can be instantiated with any of the types above.
  template<class T>
  const T* As() const {
    return type() == Type_<T>::type ? (T*)this : nullptr;
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

class Array : public Value, public std::vector<std::unique_ptr<Value>> {
 public:
  Array() {}
  Array(std::vector<std::unique_ptr<Value>>& elements);
  Array(const Array& other);

  Type type() const override { return ArrayType; }

 protected:
  void Serialize(JSONSerializer*) const override;
  std::unique_ptr<Value> Clone() const override;
};

class Object : public Value, public std::map<std::string, std::unique_ptr<Value>> {
 public:
  Object() {}
  Object(std::map<std::string, std::unique_ptr<Value>>& fields);
  Object(const Object& other);

  Type type() const override { return ObjectType; }

 protected:
  void Serialize(JSONSerializer*) const override;
  std::unique_ptr<Value> Clone() const override;
};

class JSONParser {
 public:
  static std::unique_ptr<Value> FromStream(std::istream& stream);
  static std::unique_ptr<Value> FromString(const std::string& input);
};

}

