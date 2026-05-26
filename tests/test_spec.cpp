#include "gtest/gtest.h"
#include "../src/zjson.hpp"
#include <regex>
#include <sstream>

using namespace ZJSON;

namespace {
struct UserProfile {
    std::string name;
    int age = 0;
};

void to_json(Json& json, const UserProfile& profile) {
    json = Json{{"name", profile.name}, {"age", profile.age}};
}

void from_json(const Json& json, UserProfile& profile) {
    profile.name = json["name"].toString();
    profile.age = json["age"].toInt();
}
}

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

TEST(TestSpec, parsed_string_storage_survives_source_lifetime) {
    Json parsed;
    {
        std::string input = "{\"plain\":\"alpha\",\"escaped\":\"line\\nquote\\\"\",\"arr\":[\"beta\"],\"a/b~c\":7}";
        std::string err;
        parsed = Json::ParseJsonStrict(input, err);
        ASSERT_FALSE(parsed.isError()) << err;
        input.assign(input.size(), 'x');
    }

    EXPECT_EQ(parsed["plain"].toString(), "alpha");
    EXPECT_EQ(parsed["escaped"].toString(), "line\nquote\"");
    EXPECT_EQ(parsed["arr"][0].toString(), "beta");
    EXPECT_EQ(parsed["a/b~c"].toInt(), 7);

    Json copied = parsed;
    Json moved = std::move(copied);
    EXPECT_EQ(moved["plain"].toString(), "alpha");
    EXPECT_EQ(moved["escaped"].toString(), "line\nquote\"");
    EXPECT_EQ(moved.toString(), "{\"plain\":\"alpha\",\"escaped\":\"line\\nquote\\\"\",\"arr\":[\"beta\"],\"a/b~c\":7}");
}

TEST(TestSpec, escaped_string_storage_remains_stable_under_many_values) {
    std::string input = "{\"items\":[";
    for (int i = 0; i < 300; ++i) {
        if (i > 0) input.push_back(',');
        input += "\"item_" + std::to_string(i) + "\\nvalue\"";
    }
    input += "]}";

    std::string err;
    Json parsed = Json::ParseJsonStrict(input, err);
    ASSERT_FALSE(parsed.isError()) << err;
    EXPECT_EQ(parsed["items"][0].toString(), "item_0\nvalue");
    EXPECT_EQ(parsed["items"][299].toString(), "item_299\nvalue");
}

TEST(TestSpec, parsed_storage_supports_mutation_after_parse) {
    Json parsed("{\"keep\":\"source\",\"remove\":1,\"items\":[\"a\",\"b\"]}");
    ASSERT_TRUE(parsed.isObject());

    parsed.remove("remove");
    parsed.add("added", "value");

    Json patch{{"keep", "patched"}, {"added", nullptr}, {"nested", Json{{"x", "y"}}}};
    parsed.mergePatch(patch);

    EXPECT_EQ(parsed["keep"].toString(), "patched");
    EXPECT_FALSE(parsed.contains("added"));
    EXPECT_EQ(parsed["nested"]["x"].toString(), "y");
    EXPECT_EQ(parsed.toString(), "{\"keep\":\"patched\",\"items\":[\"a\",\"b\"],\"nested\":{\"x\":\"y\"}}");
}

TEST(TestSpec, node_pool_handles_repeated_tree_create_copy_clear) {
    for (int round = 0; round < 200; ++round) {
        Json root;
        Json items(JsonType::Array);
        for (int i = 0; i < 200; ++i) {
            items.add(Json{{"id", i}, {"name", "item_" + std::to_string(i)}, {"active", i % 2 == 0}});
        }
        root.add("items", items);
        root.add("round", round);

        Json copied = root;
        EXPECT_EQ(copied["round"].toInt(), round);
        EXPECT_EQ(copied["items"][199]["name"].toString(), "item_199");

        copied.clear();
        EXPECT_EQ(copied.toString(), "{}");
    }
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
    EXPECT_TRUE(std::regex_search(err, std::regex(R"(line 1, col \d+: .+)") ));

    // Error on line 2
    err.clear();
    Json r2 = Json::ParseJson("{\n\"a\":}", err);
    EXPECT_TRUE(r2.isError());
    EXPECT_TRUE(std::regex_search(err, std::regex(R"(line 2, col \d+: .+)") ));

    // Valid JSON should produce no error
    err.clear();
    Json r3 = Json::ParseJson("{\"a\":1}", err);
    EXPECT_FALSE(r3.isError());
    EXPECT_TRUE(err.empty());
}

TEST(TestSpec, semantic_equality_and_inequality) {
    Json lhs{{"a", 1}, {"b", Json(JsonType::Array).add({2, 3})}};
    Json rhs{{"b", Json(JsonType::Array).add({2, 3})}, {"a", 1}};
    Json arrayA(JsonType::Array);
    Json arrayB(JsonType::Array);
    arrayA.add({1, 2, 3});
    arrayB.add({3, 2, 1});

    EXPECT_TRUE(lhs == rhs);
    EXPECT_FALSE(lhs != rhs);
    EXPECT_FALSE(arrayA == arrayB);
    EXPECT_TRUE(arrayA != arrayB);
}

TEST(TestSpec, pretty_print_and_stream_dump) {
    Json obj;
    obj.add("a", 1);
    obj.add("b", Json(JsonType::Array).add({true, "x"}));

    EXPECT_EQ(obj.toString(2), "{\n  \"a\": 1,\n  \"b\": [\n    true,\n    \"x\"\n  ]\n}");

    std::ostringstream compact;
    compact << obj;
    EXPECT_EQ(compact.str(), "{\"a\":1,\"b\":[true,\"x\"]}");

    std::ostringstream pretty;
    obj.dump(pretty, 2);
    EXPECT_EQ(pretty.str(), obj.toString(2));
}

TEST(TestSpec, json_pointer_lookup) {
    Json obj;
    obj.add("plain", 1);
    obj.add("a/b", 2);
    obj.add("m~n", 3);
    obj.add("arr", Json(JsonType::Array).add({Json({{"deep", 4}}), 5}));

    EXPECT_EQ(obj.at("/plain").toInt(), 1);
    EXPECT_EQ(obj.at("/a~1b").toInt(), 2);
    EXPECT_EQ(obj.at("/m~0n").toInt(), 3);
    EXPECT_EQ(obj.at("/arr/0/deep").toInt(), 4);
    EXPECT_TRUE(obj.at("/arr/x").isError());
    EXPECT_TRUE(obj.at("plain").isError());
}

TEST(TestSpec, iterator_supports_structured_bindings) {
    Json obj;
    obj.add("alpha", 1);
    obj.add("beta", 2);

    std::string keys;
    int sum = 0;
    for (auto& [key, value] : obj) {
        keys += key;
        sum += value.toInt();
    }

    const Json& constObj = obj;
    int constCount = 0;
    for (const auto& [key, value] : constObj) {
        EXPECT_FALSE(key.empty());
        EXPECT_TRUE(value.isNumber());
        ++constCount;
    }

    EXPECT_EQ(keys, "alphabeta");
    EXPECT_EQ(sum, 3);
    EXPECT_EQ(constCount, 2);
}

TEST(TestSpec, duplicate_key_policy_is_configurable) {
    const std::string input = "{\"a\":1,\"a\":2}";
    std::string err;

    Json keepLast = Json::ParseJsonStrict(input, err);
    EXPECT_FALSE(keepLast.isError()) << err;
    EXPECT_EQ(keepLast["a"].toInt(), 2);

    ParseOptions keepFirstOptions;
    keepFirstOptions.duplicateKey = ParseOptions::DuplicateKeyPolicy::KeepFirst;
    Json keepFirst = Json::ParseJsonStrict(input, err, keepFirstOptions);
    EXPECT_FALSE(keepFirst.isError()) << err;
    EXPECT_EQ(keepFirst["a"].toInt(), 1);

    ParseOptions rejectOptions;
    rejectOptions.duplicateKey = ParseOptions::DuplicateKeyPolicy::Reject;
    Json rejected = Json::ParseJsonStrict(input, err, rejectOptions);
    EXPECT_TRUE(rejected.isError());
    EXPECT_NE(err.find("duplicate key 'a'"), std::string::npos);
}

TEST(TestSpec, adl_to_json_from_json_hooks) {
    UserProfile source{"kevin", 18};
    Json json = source;
    UserProfile roundTrip = json.get<UserProfile>();

    EXPECT_EQ(json.toString(), "{\"name\":\"kevin\",\"age\":18}");
    EXPECT_EQ(roundTrip.name, "kevin");
    EXPECT_EQ(roundTrip.age, 18);
}

TEST(TestSpec, json_merge_patch_applies_rfc7396_rules) {
    Json target{
        {"title", "Goodbye!"},
        {"author", Json{{"givenName", "John"}, {"familyName", "Doe"}}},
        {"tags", Json(JsonType::Array).add({"example", "sample"})},
        {"content", "This will be unchanged"}
    };

    Json patch{
        {"title", "Hello!"},
        {"phoneNumber", "+01-123-456-7890"},
        {"author", Json{{"familyName", nullptr}}},
        {"tags", Json(JsonType::Array).add({"example"})}
    };

    target.mergePatch(patch);

    EXPECT_EQ(target.toString(), "{\"title\":\"Hello!\",\"author\":{\"givenName\":\"John\"},\"tags\":[\"example\"],\"content\":\"This will be unchanged\",\"phoneNumber\":\"+01-123-456-7890\"}");

    Json scalarBase(5);
    scalarBase.mergePatch(Json{{"created", true}});
    EXPECT_EQ(scalarBase.toString(), "{\"created\":true}");
}

TEST(TestSpec, json_patch_applies_rfc6902_operations) {
    Json base{
        {"foo", "bar"},
        {"numbers", Json(JsonType::Array).add({1, 2, 3})},
        {"nested", Json{{"x", 1}}}
    };

    Json ops(JsonType::Array);
    ops.add(Json{{"op", "add"}, {"path", "/numbers/-"}, {"value", 4}});
    ops.add(Json{{"op", "replace"}, {"path", "/foo"}, {"value", "baz"}});
    ops.add(Json{{"op", "add"}, {"path", "/nested/y"}, {"value", true}});
    ops.add(Json{{"op", "copy"}, {"from", "/nested/y"}, {"path", "/copied"}});
    ops.add(Json{{"op", "move"}, {"from", "/numbers/0"}, {"path", "/numbers/2"}});
    ops.add(Json{{"op", "test"}, {"path", "/nested/x"}, {"value", 1}});
    ops.add(Json{{"op", "remove"}, {"path", "/nested/x"}});

    std::string err;
    Json patched = base.applyPatch(ops, err);

    EXPECT_FALSE(patched.isError()) << err;
    EXPECT_TRUE(err.empty());
    EXPECT_EQ(patched.toString(), "{\"foo\":\"baz\",\"numbers\":[2,3,1,4],\"nested\":{\"y\":true},\"copied\":true}");
}

TEST(TestSpec, json_patch_reports_failures) {
    Json base{{"foo", "bar"}};
    Json failingOps(JsonType::Array);
    failingOps.add(Json{{"op", "test"}, {"path", "/foo"}, {"value", "baz"}});

    std::string err;
    Json failed = base.applyPatch(failingOps, err);

    EXPECT_TRUE(failed.isError());
    EXPECT_NE(err.find("test operation failed"), std::string::npos);
}
