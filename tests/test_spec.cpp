#include "gtest/gtest.h"
#include "../src/zjson.hpp"

using namespace ZJSON;

TEST(TestSpec, stringify_escape_characters) {
    Json obj;
    obj.add("quote", "a\"b");
    obj.add("slash", "c\\d");
    obj.add("ctrl", std::string("x\n\t\r\b\f", 6));

    EXPECT_EQ(obj.toString(), "{\"quote\":\"a\\\"b\",\"slash\":\"c\\\\d\",\"ctrl\":\"x\\n\\t\\r\\b\\f\"}");
}

TEST(TestSpec, parse_invalid_json_fallback_to_string) {
    Json invalid1("[1,]");
    Json invalid2("{\"a\":}");
    Json invalid3("[01]");

    EXPECT_TRUE(invalid1.isString());
    EXPECT_TRUE(invalid2.isString());
    EXPECT_TRUE(invalid3.isString());
}

TEST(TestSpec, parse_with_comments_extension) {
    Json data("{\"a\":1,/*comment*/\"b\":2}");
    EXPECT_TRUE(data.isObject());
    EXPECT_EQ(data["a"].toInt(), 1);
    EXPECT_EQ(data["b"].toInt(), 2);
}

TEST(TestSpec, parse_depth_limit) {
    std::string deep;
    for (int i = 0; i < 120; ++i) {
        deep.push_back('[');
    }
    for (int i = 0; i < 120; ++i) {
        deep.push_back(']');
    }

    Json data(deep);
    EXPECT_TRUE(data.isString());
}

TEST(TestSpec, copy_and_move_assignment_safety) {
    Json origin;
    origin.add("k1", 1);
    origin.add("k2", Json(JsonType::Array).add({1, 2, 3}));

    Json copied;
    copied = origin;
    EXPECT_EQ(copied.toString(), "{\"k1\":1,\"k2\":[1,2,3]}");

    Json moved;
    moved = std::move(copied);
    EXPECT_EQ(moved.toString(), "{\"k1\":1,\"k2\":[1,2,3]}");
}

TEST(TestSpec, key_name_escaping) {
    // Keys containing special characters must be escaped in JSON output
    Json obj;
    obj.add("normal", 1);
    obj.add("has\"quote", 2);
    obj.add("has\\slash", 3);
    obj.add("has\nnewline", 4);
    obj.add("has\ttab", 5);

    std::string out = obj.toString();
    // Verify the output is valid JSON by round-tripping
    EXPECT_NE(out.find("\"has\\\"quote\""), std::string::npos);
    EXPECT_NE(out.find("\"has\\\\slash\""), std::string::npos);
    EXPECT_NE(out.find("\"has\\nnewline\""), std::string::npos);
    EXPECT_NE(out.find("\"has\\ttab\""), std::string::npos);

    // Round-trip: parse the output and verify values
    Json reparsed(out);
    EXPECT_TRUE(reparsed.isObject());
    EXPECT_EQ(reparsed["normal"].toInt(), 1);
    EXPECT_EQ(reparsed["has\"quote"].toInt(), 2);
    EXPECT_EQ(reparsed["has\\slash"].toInt(), 3);
    EXPECT_EQ(reparsed["has\nnewline"].toInt(), 4);
    EXPECT_EQ(reparsed["has\ttab"].toInt(), 5);
}

TEST(TestSpec, key_escaping_for_nested_objects) {
    // Key escaping must also work for nested object/array children in toString()
    Json inner;
    inner.add("val", 42);
    Json outer;
    outer.add("key\"with\"quotes", inner);
    std::string out = outer.toString();
    EXPECT_NE(out.find("\"key\\\"with\\\"quotes\""), std::string::npos);

    Json reparsed(out);
    EXPECT_TRUE(reparsed.isObject());
    EXPECT_EQ(reparsed["key\"with\"quotes"]["val"].toInt(), 42);
}

TEST(TestSpec, empty_array_subscript_access) {
    Json emptyArr(JsonType::Array);
    EXPECT_TRUE(emptyArr.isEmpty());
    // Accessing any index on an empty array must return Error, not crash
    Json r0 = emptyArr[0];
    EXPECT_TRUE(r0.isError());
    Json r1 = emptyArr[1];
    EXPECT_TRUE(r1.isError());
    Json rNeg = emptyArr[-1];
    EXPECT_TRUE(rNeg.isError());
}

TEST(TestSpec, wide_object_copy) {
    // Verify that copying a wide object (many keys) doesn't stack-overflow
    Json wide;
    for (int i = 0; i < 2000; ++i) {
        wide.add("k" + std::to_string(i), i);
    }
    EXPECT_EQ(wide.size(), -1);  // size() returns -1 for objects

    Json copied = wide;
    EXPECT_EQ(copied["k0"].toInt(), 0);
    EXPECT_EQ(copied["k999"].toInt(), 999);
    EXPECT_EQ(copied["k1999"].toInt(), 1999);
}

TEST(TestSpec, parse_error_reports_position) {
    std::string err;
    // Error at col 4: missing colon
    Json r1 = Json::ParseJson("{\"a\" 1}", err);
    EXPECT_TRUE(r1.isError());
    EXPECT_NE(err.find("line 1"), std::string::npos);
    EXPECT_NE(err.find("col "), std::string::npos);

    // Error on line 2
    err.clear();
    Json r2 = Json::ParseJson("{\n\"a\":}", err);
    EXPECT_TRUE(r2.isError());
    EXPECT_NE(err.find("line 2"), std::string::npos);

    // Valid JSON should produce no error
    err.clear();
    Json r3 = Json::ParseJson("{\"a\":1}", err);
    EXPECT_FALSE(r3.isError());
    EXPECT_TRUE(err.empty());
}
