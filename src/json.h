#include <istream>
#include <ostream>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace json {

class JSONSerializer;

// A JSON value. All other JSON values inherit from Value.
class Value {
 public:
  virtual ~Value() {}

  std::string ToJSON();

  virtual std::unique_ptr<Value> Clone() const = 0;

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

 protected:
  void Serialize(JSONSerializer*) const override;
  std::unique_ptr<Value> Clone() const override;
};

class Boolean : public Value {
 public:
  Boolean(bool value) : value_(value) {}
  Boolean(const Boolean&) = default;

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

 protected:
  void Serialize(JSONSerializer*) const override;
  std::unique_ptr<Value> Clone() const override;
};

class Object : public Value, public std::map<std::string, std::unique_ptr<Value>> {
 public:
  Object() {}
  Object(std::map<std::string, std::unique_ptr<Value>>& fields);
  Object(const Object& other);

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

