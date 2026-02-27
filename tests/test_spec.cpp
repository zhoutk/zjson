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
