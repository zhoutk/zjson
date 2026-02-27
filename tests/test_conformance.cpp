#include "gtest/gtest.h"
#include "../src/zjson.hpp"
#include <vector>

using namespace ZJSON;

namespace {

bool parses_as_json_container(const std::string& input) {
    Json j(input);
    return j.isObject() || j.isArray();
}

} // namespace

TEST(TestConformance, valid_cases) {
    const std::vector<std::string> valids = {
        "[]",
        "{}",
        "[0,1,2]",
        "[0e1,-12.34E+5,1.2e-3]",
        "{\"a\":1,\"b\":true,\"c\":null,\"d\":[1,2,3]}",
        "{\"dup\":1,\"dup\":2}",
        "{\"esc\":\"\\\\ \\\" \\n \\t \\r \\b \\f\"}",
        "{\"unicode\":\"\\u4F60\\u597D\"}",
        "{\"pair\":\"\\uD834\\uDD1E\"}",
        "{\"comment-ok\":1,/*ext*/\"b\":2}",
        "[\n\t1,2,3\r\n]"
    };

    for (const auto& text : valids) {
        EXPECT_TRUE(parses_as_json_container(text)) << text;
    }
}

TEST(TestConformance, invalid_cases) {
    const std::vector<std::string> invalids = {
        "",
        "[1,]",
        "{\"a\":1,}",
        "[.1]",
        "[1.]",
        "[01]",
        "[1e]",
        "[1e+]",
        "[--1]",
        "[+1]",
        "[NaN]",
        "[Infinity]",
        "{\"a\" 1}",
        "{\"a\":}",
        "{key:1}",
        "[\"bad\\x00\"]",
        "[\"bad\\u12G4\"]",
        "[\"bad\\u123\"]",
        "{\"a\":/*comment*/}",
        "[1 2]"
    };

    for (const auto& text : invalids) {
        EXPECT_FALSE(parses_as_json_container(text)) << text;
    }
}
