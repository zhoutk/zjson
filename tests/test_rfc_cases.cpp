#include "gtest/gtest.h"
#include "../src/zjson.hpp"
#include <vector>

using namespace ZJSON;

namespace {

bool parse_ok_ext(const std::string& input) {
    std::string err;
    Json j = Json::ParseJson(input, err);
    return !j.isError();
}

bool parse_ok_strict(const std::string& input) {
    std::string err;
    Json j = Json::ParseJsonStrict(input, err);
    return !j.isError();
}

} // namespace

TEST(TestRfcCases, strict_mode_valid_inputs) {
    const std::vector<std::string> valids = {
        "null",
        "true",
        "false",
        "0",
        "-0",
        "123",
        "-12.34",
        "1e10",
        "0E0",
        "\"hello\"",
        "[]",
        "{}",
        "[1,2,3]",
        "{\"a\":1,\"b\":[true,false,null],\"c\":{\"x\":\"y\"}}",
        "{\"\":0}",
        "[\"\\uD834\\uDD1E\"]",
        " \t\r\n {\"a\":1} \n"
    };

    for (const auto& text : valids) {
        EXPECT_TRUE(parse_ok_strict(text)) << text;
    }
}

TEST(TestRfcCases, strict_mode_invalid_inputs) {
    const std::vector<std::string> invalids = {
        "",
        " ",
        "[1,]",
        "{\"a\":1,}",
        "{key:1}",
        "{\"a\":/*c*/1}",
        "[NaN]",
        "[Infinity]",
        "[01]",
        "[1.]",
        "[.1]",
        "[1e]",
        "[1e+]",
        "[1eE2]",
        "[\"bad\\x00\"]",
        "[\"bad\\u12G4\"]",
        "[\"bad\\u123\"]",
        "{\"a\":1}x",
        "[1 2]",
        "{\"a\" 1}",
        "{\"a\":}",
        std::string("[\f1]"),
        std::string("\xE2\x81\xA0{}")
    };

    for (const auto& text : invalids) {
        EXPECT_FALSE(parse_ok_strict(text)) << text;
    }
}

TEST(TestRfcCases, extension_mode_allows_comments) {
    const std::vector<std::string> comment_json = {
        "/*a*/{\"k\":1}",
        "{\"a\":1,/*mid*/\"b\":2}",
        "[1,/*x*/2,3]",
        "[1]//tail\n"
    };

    for (const auto& text : comment_json) {
        EXPECT_TRUE(parse_ok_ext(text)) << text;
        EXPECT_FALSE(parse_ok_strict(text)) << text;
    }
}

TEST(TestRfcCases, duplicate_keys_first_wins_in_lookup) {
    std::string err;
    Json j = Json::ParseJsonStrict("{\"a\":1,\"a\":2}", err);
    ASSERT_FALSE(j.isError()) << err;
    EXPECT_EQ(j["a"].toInt(), 1);
}

TEST(TestRfcCases, stress_parse_stringify_copy_loop) {
    const std::string sample = "{\"a\":[1,2,3],\"b\":{\"c\":\"text\",\"d\":-12.34e+5},\"e\":true}";

    for (int i = 0; i < 2000; ++i) {
        std::string err;
        Json parsed = Json::ParseJsonStrict(sample, err);
        ASSERT_FALSE(parsed.isError()) << err;

        Json copied = parsed;
        Json moved = std::move(copied);

        std::string out = moved.toString();
        Json reparsed = Json::ParseJsonStrict(out, err);
        ASSERT_FALSE(reparsed.isError()) << err;
        ASSERT_EQ(reparsed["a"][2].toInt(), 3);
        ASSERT_EQ(reparsed["b"]["c"].toString(), "text");
    }
}
