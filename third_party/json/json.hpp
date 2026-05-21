// MonoSprite JSON Library
// A minimal JSON parser/serializer for AI API communication.
// License: MIT
//
// This is a lightweight, header-only JSON implementation providing
// parse, serialize, object/array access for the AI client module.

#ifndef MONOSPRITE_JSON_HPP
#define MONOSPRITE_JSON_HPP

#include <string>
#include <vector>
#include <map>
#include <sstream>
#include <stdexcept>
#include <cstdlib>
#include <cassert>

namespace json {

enum class Type {
  Null,
  Bool,
  Number,
  String,
  Array,
  Object
};

class Value {
public:
  Value() : m_type(Type::Null) {}

  explicit Value(bool v) : m_type(Type::Bool), m_bool(v) {}

  explicit Value(int v) : m_type(Type::Number), m_number(static_cast<double>(v)) {}

  explicit Value(double v) : m_type(Type::Number), m_number(v) {}

  explicit Value(const std::string& v) : m_type(Type::String), m_string(v) {}

  explicit Value(const char* v) : m_type(Type::String), m_string(v ? v : "") {}

  // Type queries
  Type type() const { return m_type; }
  bool isNull() const { return m_type == Type::Null; }
  bool isBool() const { return m_type == Type::Bool; }
  bool isNumber() const { return m_type == Type::Number; }
  bool isString() const { return m_type == Type::String; }
  bool isArray() const { return m_type == Type::Array; }
  bool isObject() const { return m_type == Type::Object; }

  // Value accessors
  bool asBool() const {
    if (m_type == Type::Bool) return m_bool;
    return false;
  }

  double asNumber() const {
    if (m_type == Type::Number) return m_number;
    return 0.0;
  }

  int asInt() const {
    return static_cast<int>(asNumber());
  }

  const std::string& asString() const {
    static const std::string empty;
    if (m_type == Type::String) return m_string;
    return empty;
  }

  // Array operations
  static Value makeArray() {
    Value v;
    v.m_type = Type::Array;
    return v;
  }

  void push_back(const Value& val) {
    if (m_type != Type::Array) {
      m_type = Type::Array;
      m_array.clear();
    }
    m_array.push_back(val);
  }

  size_t size() const {
    if (m_type == Type::Array) return m_array.size();
    if (m_type == Type::Object) return m_object.size();
    return 0;
  }

  const Value& operator[](size_t index) const {
    static const Value null_value;
    if (m_type == Type::Array && index < m_array.size())
      return m_array[index];
    return null_value;
  }

  Value& operator[](size_t index) {
    if (m_type != Type::Array) {
      m_type = Type::Array;
      m_array.clear();
    }
    if (index >= m_array.size())
      m_array.resize(index + 1);
    return m_array[index];
  }

  const std::vector<Value>& arrayItems() const { return m_array; }

  // Object operations
  static Value makeObject() {
    Value v;
    v.m_type = Type::Object;
    return v;
  }

  const Value& operator[](const std::string& key) const {
    static const Value null_value;
    if (m_type == Type::Object) {
      auto it = m_object.find(key);
      if (it != m_object.end())
        return it->second;
    }
    return null_value;
  }

  Value& operator[](const std::string& key) {
    if (m_type != Type::Object) {
      m_type = Type::Object;
      m_object.clear();
    }
    return m_object[key];
  }

  bool hasKey(const std::string& key) const {
    if (m_type != Type::Object) return false;
    return m_object.find(key) != m_object.end();
  }

  const std::map<std::string, Value>& objectItems() const { return m_object; }

  // Serialization
  std::string serialize() const {
    std::ostringstream oss;
    serializeImpl(oss);
    return oss.str();
  }

  // Parsing
  static Value parse(const std::string& input) {
    size_t pos = 0;
    Value result = parseValue(input, pos);
    return result;
  }

private:
  Type m_type;
  bool m_bool = false;
  double m_number = 0.0;
  std::string m_string;
  std::vector<Value> m_array;
  std::map<std::string, Value> m_object;

  void serializeImpl(std::ostringstream& oss) const {
    switch (m_type) {
      case Type::Null:
        oss << "null";
        break;
      case Type::Bool:
        oss << (m_bool ? "true" : "false");
        break;
      case Type::Number:
        if (m_number == static_cast<int>(m_number))
          oss << static_cast<int>(m_number);
        else
          oss << m_number;
        break;
      case Type::String:
        oss << "\"";
        for (char c : m_string) {
          switch (c) {
            case '"':  oss << "\\\""; break;
            case '\\': oss << "\\\\"; break;
            case '\b': oss << "\\b"; break;
            case '\f': oss << "\\f"; break;
            case '\n': oss << "\\n"; break;
            case '\r': oss << "\\r"; break;
            case '\t': oss << "\\t"; break;
            default:   oss << c;
          }
        }
        oss << "\"";
        break;
      case Type::Array:
        oss << "[";
        for (size_t i = 0; i < m_array.size(); ++i) {
          if (i > 0) oss << ",";
          m_array[i].serializeImpl(oss);
        }
        oss << "]";
        break;
      case Type::Object:
        oss << "{";
        {
          bool first = true;
          for (auto& kv : m_object) {
            if (!first) oss << ",";
            first = false;
            oss << "\"";
            for (char c : kv.first) {
              if (c == '"') oss << "\\\"";
              else if (c == '\\') oss << "\\\\";
              else oss << c;
            }
            oss << "\":";
            kv.second.serializeImpl(oss);
          }
        }
        oss << "}";
        break;
    }
  }

  // ---- Parser implementation ----

  static void skipWhitespace(const std::string& s, size_t& pos) {
    while (pos < s.size() && (s[pos] == ' ' || s[pos] == '\t' ||
                              s[pos] == '\n' || s[pos] == '\r'))
      ++pos;
  }

  static Value parseValue(const std::string& s, size_t& pos) {
    skipWhitespace(s, pos);
    if (pos >= s.size())
      return Value();

    char c = s[pos];
    if (c == '"')       return parseString(s, pos);
    if (c == '{')       return parseObject(s, pos);
    if (c == '[')       return parseArray(s, pos);
    if (c == 't' || c == 'f') return parseBool(s, pos);
    if (c == 'n')       return parseNull(s, pos);
    if (c == '-' || (c >= '0' && c <= '9')) return parseNumber(s, pos);

    return Value(); // fallback
  }

  static Value parseString(const std::string& s, size_t& pos) {
    assert(s[pos] == '"');
    ++pos; // skip opening quote
    std::string result;
    while (pos < s.size() && s[pos] != '"') {
      if (s[pos] == '\\') {
        ++pos;
        if (pos >= s.size()) break;
        switch (s[pos]) {
          case '"':  result += '"'; break;
          case '\\': result += '\\'; break;
          case '/':  result += '/'; break;
          case 'b':  result += '\b'; break;
          case 'f':  result += '\f'; break;
          case 'n':  result += '\n'; break;
          case 'r':  result += '\r'; break;
          case 't':  result += '\t'; break;
          case 'u': {
            // Parse 4 hex digits (simplified: ASCII only)
            if (pos + 4 < s.size()) {
              std::string hex = s.substr(pos + 1, 4);
              unsigned int codepoint = std::stoul(hex, nullptr, 16);
              if (codepoint < 0x80) {
                result += static_cast<char>(codepoint);
              } else if (codepoint < 0x800) {
                result += static_cast<char>(0xC0 | (codepoint >> 6));
                result += static_cast<char>(0x80 | (codepoint & 0x3F));
              } else {
                result += static_cast<char>(0xE0 | (codepoint >> 12));
                result += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
                result += static_cast<char>(0x80 | (codepoint & 0x3F));
              }
              pos += 4;
            }
            break;
          }
          default: result += s[pos]; break;
        }
      } else {
        result += s[pos];
      }
      ++pos;
    }
    if (pos < s.size()) ++pos; // skip closing quote
    return Value(result);
  }

  static Value parseNumber(const std::string& s, size_t& pos) {
    size_t start = pos;
    if (s[pos] == '-') ++pos;
    while (pos < s.size() && s[pos] >= '0' && s[pos] <= '9') ++pos;
    if (pos < s.size() && s[pos] == '.') {
      ++pos;
      while (pos < s.size() && s[pos] >= '0' && s[pos] <= '9') ++pos;
    }
    if (pos < s.size() && (s[pos] == 'e' || s[pos] == 'E')) {
      ++pos;
      if (pos < s.size() && (s[pos] == '+' || s[pos] == '-')) ++pos;
      while (pos < s.size() && s[pos] >= '0' && s[pos] <= '9') ++pos;
    }
    double val = std::strtod(s.c_str() + start, nullptr);
    return Value(val);
  }

  static Value parseBool(const std::string& s, size_t& pos) {
    if (s.compare(pos, 4, "true") == 0) {
      pos += 4;
      return Value(true);
    }
    if (s.compare(pos, 5, "false") == 0) {
      pos += 5;
      return Value(false);
    }
    return Value();
  }

  static Value parseNull(const std::string& s, size_t& pos) {
    if (s.compare(pos, 4, "null") == 0) {
      pos += 4;
    }
    return Value();
  }

  static Value parseArray(const std::string& s, size_t& pos) {
    assert(s[pos] == '[');
    ++pos;
    Value arr = Value::makeArray();
    skipWhitespace(s, pos);
    if (pos < s.size() && s[pos] == ']') {
      ++pos;
      return arr;
    }
    while (pos < s.size()) {
      arr.push_back(parseValue(s, pos));
      skipWhitespace(s, pos);
      if (pos < s.size() && s[pos] == ',') {
        ++pos;
        skipWhitespace(s, pos);
      } else {
        break;
      }
    }
    if (pos < s.size() && s[pos] == ']') ++pos;
    return arr;
  }

  static Value parseObject(const std::string& s, size_t& pos) {
    assert(s[pos] == '{');
    ++pos;
    Value obj = Value::makeObject();
    skipWhitespace(s, pos);
    if (pos < s.size() && s[pos] == '}') {
      ++pos;
      return obj;
    }
    while (pos < s.size()) {
      skipWhitespace(s, pos);
      if (pos >= s.size() || s[pos] != '"') break;
      Value key = parseString(s, pos);
      skipWhitespace(s, pos);
      if (pos < s.size() && s[pos] == ':') ++pos;
      obj[key.asString()] = parseValue(s, pos);
      skipWhitespace(s, pos);
      if (pos < s.size() && s[pos] == ',') {
        ++pos;
        skipWhitespace(s, pos);
      } else {
        break;
      }
    }
    if (pos < s.size() && s[pos] == '}') ++pos;
    return obj;
  }
};

} // namespace json

#endif // MONOSPRITE_JSON_HPP
