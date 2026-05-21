// MonoSprite JSON Library Tests
// Copyright (C) 2026 MonoSprite contributors
//
// This file is released under the terms of the MIT license.
// Read LICENSE.txt for more information.

#include <gtest/gtest.h>
#include "json/json.hpp"

TEST(JsonValue, TypeAndAccess)
{
  json::Value valNull;
  EXPECT_TRUE(valNull.isNull());
  EXPECT_EQ(json::Type::Null, valNull.type());

  json::Value valBool(true);
  EXPECT_TRUE(valBool.isBool());
  EXPECT_TRUE(valBool.asBool());

  json::Value valInt(42);
  EXPECT_TRUE(valInt.isNumber());
  EXPECT_EQ(42, valInt.asInt());
  EXPECT_DOUBLE_EQ(42.0, valInt.asNumber());

  json::Value valDouble(3.14);
  EXPECT_TRUE(valDouble.isNumber());
  EXPECT_DOUBLE_EQ(3.14, valDouble.asNumber());
  EXPECT_EQ(3, valDouble.asInt());

  json::Value valStr("hello");
  EXPECT_TRUE(valStr.isString());
  EXPECT_EQ("hello", valStr.asString());
}

TEST(JsonValue, ArrayOperations)
{
  json::Value arr = json::Value::makeArray();
  EXPECT_TRUE(arr.isArray());
  EXPECT_EQ(0, arr.size());

  arr.push_back(json::Value(1));
  arr.push_back(json::Value("two"));
  arr.push_back(json::Value(true));

  EXPECT_EQ(3, arr.size());
  EXPECT_EQ(1, arr[0].asInt());
  EXPECT_EQ("two", arr[1].asString());
  EXPECT_TRUE(arr[2].asBool());

  // Check out of bounds safety
  EXPECT_TRUE(arr[3].isNull());
}

TEST(JsonValue, ObjectOperations)
{
  json::Value obj = json::Value::makeObject();
  EXPECT_TRUE(obj.isObject());
  EXPECT_EQ(0, obj.size());

  obj["key1"] = json::Value("value1");
  obj["key2"] = json::Value(123);
  obj["key3"] = json::Value(false);

  EXPECT_EQ(3, obj.size());
  EXPECT_TRUE(obj.hasKey("key1"));
  EXPECT_TRUE(obj.hasKey("key2"));
  EXPECT_TRUE(obj.hasKey("key3"));
  EXPECT_FALSE(obj.hasKey("key4"));

  EXPECT_EQ("value1", obj["key1"].asString());
  EXPECT_EQ(123, obj["key2"].asInt());
  EXPECT_FALSE(obj["key3"].asBool());

  // Accessing missing key returns null
  EXPECT_TRUE(obj["missing"].isNull());
}

TEST(JsonValue, Serialization)
{
  // Simple values
  EXPECT_EQ("null", json::Value().serialize());
  EXPECT_EQ("true", json::Value(true).serialize());
  EXPECT_EQ("false", json::Value(false).serialize());
  EXPECT_EQ("123", json::Value(123).serialize());
  EXPECT_EQ("3.14", json::Value(3.14).serialize());
  EXPECT_EQ("\"hello\"", json::Value("hello").serialize());
  EXPECT_EQ("\"line1\\nline2\"", json::Value("line1\nline2").serialize());

  // Array
  json::Value arr = json::Value::makeArray();
  arr.push_back(json::Value(1));
  arr.push_back(json::Value("two"));
  EXPECT_EQ("[1,\"two\"]", arr.serialize());

  // Object
  json::Value obj = json::Value::makeObject();
  obj["a"] = json::Value(1);
  obj["b"] = json::Value("c");
  // std::map maintains keys sorted alphabetically: "a", "b"
  EXPECT_EQ("{\"a\":1,\"b\":\"c\"}", obj.serialize());
}

TEST(JsonValue, Parsing)
{
  // Simple values
  EXPECT_TRUE(json::Value::parse("null").isNull());
  EXPECT_TRUE(json::Value::parse("true").asBool());
  EXPECT_FALSE(json::Value::parse("false").asBool());
  EXPECT_EQ(123, json::Value::parse("123").asInt());
  EXPECT_DOUBLE_EQ(3.14, json::Value::parse("3.14").asNumber());
  EXPECT_EQ("hello", json::Value::parse("\"hello\"").asString());
  EXPECT_EQ("escaped\"quotes", json::Value::parse("\"escaped\\\"quotes\"").asString());
  EXPECT_EQ("line1\nline2", json::Value::parse("\"line1\\nline2\"").asString());

  // Unicode parsing
  EXPECT_EQ("A", json::Value::parse("\"\\u0041\"").asString());

  // Whitespace tolerance
  EXPECT_EQ(42, json::Value::parse("   42   ").asInt());

  // Array parsing
  json::Value arr = json::Value::parse("  [ 1 , \"two\" , true ]  ");
  EXPECT_TRUE(arr.isArray());
  EXPECT_EQ(3, arr.size());
  EXPECT_EQ(1, arr[0].asInt());
  EXPECT_EQ("two", arr[1].asString());
  EXPECT_TRUE(arr[2].asBool());

  // Object parsing
  json::Value obj = json::Value::parse(" { \"x\" : 10 , \"y\" : \"val\" } ");
  EXPECT_TRUE(obj.isObject());
  EXPECT_EQ(2, obj.size());
  EXPECT_EQ(10, obj["x"].asInt());
  EXPECT_EQ("val", obj["y"].asString());

  // Nested parsing
  json::Value complex = json::Value::parse("{\"list\":[1,2,{\"ok\":true}],\"status\":\"success\"}");
  EXPECT_TRUE(complex.isObject());
  EXPECT_EQ("success", complex["status"].asString());
  EXPECT_TRUE(complex["list"].isArray());
  EXPECT_EQ(3, complex["list"].size());
  EXPECT_TRUE(complex["list"][2]["ok"].asBool());
}

int main(int argc, char** argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
