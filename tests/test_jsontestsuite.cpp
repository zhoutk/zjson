// JSONTestSuite subset regression tests
// Based on: https://github.com/nst/JSONTestSuite
// y_ = MUST accept, n_ = MUST reject, i_ = implementation-defined
//
// This file embeds representative test vectors inline so no external
// JSON fixture files are required.

#include "gtest/gtest.h"
#include "../src/zjson.hpp"
#include <string>
#include <vector>

using namespace ZJSON;

// ---------- helpers ----------
static bool must_accept(const std::string& input) {
    std::string err;
    Json j = Json::ParseJsonStrict(input, err);
    return !j.isError();
}

static bool must_reject(const std::string& input) {
    std::string err;
    Json j = Json::ParseJsonStrict(input, err);
    return j.isError();
}

static bool must_accept_ext(const std::string& input) {
    std::string err;
    Json j = Json::ParseJson(input, err);
    return !j.isError();
}

// ========================================================================
//  y_ (MUST accept) — representative subset of JSONTestSuite y_* vectors
// ========================================================================
TEST(JSONTestSuite, y_must_accept) {
    struct TC { const char* name; std::string input; };
    std::vector<TC> cases = {
        // ---- structure ----
        {"y_array_empty",                       "[]"},
        {"y_object_empty",                      "{}"},
        {"y_structure_lonely_false",            "false"},
        {"y_structure_lonely_true",             "true"},
        {"y_structure_lonely_null",             "null"},
        {"y_structure_lonely_int",              "42"},
        {"y_structure_lonely_negative_real",    "-0.1"},
        {"y_structure_lonely_string",           "\"asd\""},
        {"y_structure_string_empty",            "\"\""},
        {"y_structure_true_in_array",           "[true]"},
        {"y_structure_whitespace_array",        " [] "},
        {"y_structure_trailing_newline",        "[\"a\"]\n"},

        // ---- numbers ----
        {"y_number_0e1",                        "[0e1]"},
        {"y_number_after_space",                "[ 4]"},
        {"y_number_int_with_exp",               "[20e1]"},
        {"y_number_negative_int",               "[-123]"},
        {"y_number_negative_zero",              "[-0]"},
        {"y_number_real_capital_e",             "[1E22]"},
        {"y_number_real_exponent",              "[123e45]"},
        {"y_number_real_fraction_exponent",     "[123.456e78]"},
        {"y_number_simple_int",                 "[123]"},
        {"y_number_simple_real",                "[123.456789]"},
        {"y_number_minus_zero",                 "[-0]"},
        {"y_number_double_close_to_zero",       "[0.000000000000000000001]"},
        {"y_number_real_neg_exp",               "[1e-2]"},
        {"y_number_real_pos_exp",               "[1E+2]"},

        // ---- strings ----
        {"y_string_allowed_escapes",            "[\"\\\"\\\\\\//\\b\\f\\n\\r\\t\"]"},
        {"y_string_space",                      "\" \""},
        {"y_string_simple_ascii",               "[\"asd\"]"},
        {"y_string_pi",                         "[\"\\u03c0\"]"},
        {"y_string_null_escape",                "[\"\\u0000\"]"},
        {"y_string_surrogates_musical_symbol",  "[\"\\uD834\\uDD1E\"]"},
        {"y_string_unicode_escaped_dbl_quote",  "[\"\\u0022\"]"},
        {"y_string_escaped_noncharacter",       "[\"\\uFFFF\"]"},
        {"y_string_unicode_U+200B_ZWSP",        "[\"\\u200B\"]"},
        {"y_string_two_byte_utf8",              "[\"\xc3\xa9\"]"},   // é
        {"y_string_three_byte_utf8",            "[\"\xe2\x80\xa8\"]"}, // U+2028
        {"y_string_four_byte_utf8",             "[\"\xf0\x9d\x84\x9e\"]"}, // U+1D11E
        {"y_string_del_character",              std::string("[\"a") + "\x7f" + "a\"]"},
        {"y_string_backslash_doublequotes",     "[\"\\\"asd\\\"\"]"},

        // ---- objects ----
        {"y_object_basic",                      "{\"asd\":\"sdf\"}"},
        {"y_object_duplicated_key",             "{\"a\":\"b\",\"a\":\"c\"}"},
        {"y_object_empty_key",                  "{\"\":0}"},
        {"y_object_long_strings",               "{\"x\":[{\"id\":\"xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx\"}]}"},
        {"y_object_extreme_numbers",            "{\"min\":-1.0e+28,\"max\":1.0e+28}"},

        // ---- arrays ----
        {"y_array_with_1_and_newline",          "[1\n]"},
        {"y_array_with_several_null",           "[null,null,null]"},
        {"y_array_heterogeneous",               "[null,1,\"1\",{}]"},
        {"y_array_with_trailing_space",         "[2] "},
        {"y_array_with_leading_space",          "  [2]"},
    };

    for (const auto& tc : cases) {
        EXPECT_TRUE(must_accept(tc.input))
            << "FAIL y_: " << tc.name << "  input: " << tc.input;
    }
}

// ========================================================================
//  n_ (MUST reject) — representative subset of JSONTestSuite n_* vectors
// ========================================================================
TEST(JSONTestSuite, n_must_reject) {
    struct TC { const char* name; std::string input; };
    std::vector<TC> cases = {
        // ---- structure ----
        {"n_structure_no_data",                     ""},
        {"n_single_space",                          " "},
        {"n_structure_double_array",                "[][]"},
        {"n_structure_end_array",                   "]"},
        {"n_structure_close_unopened_array",         "1]"},
        {"n_structure_object_followed_by_close",    "{}}"},
        {"n_structure_lone_open_bracket",            "["},
        {"n_structure_open_object",                 "{"},
        {"n_structure_open_object_close_array",     "{]"},
        {"n_structure_array_trailing_garbage",      "[1]x"},
        {"n_structure_array_with_extra_close",      "[1]]"},
        {"n_structure_number_with_trailing_garbage", "2@"},
        {"n_structure_object_with_trailing_garbage", "{\"a\": true} \"x\""},
        {"n_structure_capitalized_True",            "[True]"},
        {"n_structure_single_star",                 "*"},
        {"n_structure_angle_bracket",               "<.>"},
        {"n_structure_open_array_open_string",      "[\"a"},
        {"n_structure_unclosed_array",              "[1"},
        {"n_structure_unclosed_object",             "{\"asd\":\"asd\""},
        {"n_structure_open_array_comma",            "[,"},
        {"n_structure_open_object_comma",           "{,"},

        // ---- arrays ----
        {"n_array_1_true_without_comma",            "[1 true]"},
        {"n_array_extra_comma",                     "[\"\",]"},
        {"n_array_double_comma",                    "[1,,2]"},
        {"n_array_just_comma",                      "[,]"},
        {"n_array_just_minus",                      "[-]"},
        {"n_array_missing_value",                   "[   , \"\"]"},
        {"n_array_number_and_comma",                "[1,]"},
        {"n_array_star_inside",                     "[*]"},
        {"n_array_colon_instead_of_comma",          "[\"\":1]"},
        {"n_array_items_separated_by_semicolon",    "[1:2]"},
        {"n_array_inner_array_no_comma",            "[3[4]]"},

        // ---- numbers ----
        {"n_number_plus_1",                         "[+1]"},
        {"n_number_plus_Inf",                       "[+Inf]"},
        {"n_number_minus_01",                       "[-01]"},
        {"n_number_minus_NaN",                      "[-NaN]"},
        {"n_number_dot_2e3",                        "[.2e-3]"},
        {"n_number_0_dot_1_dot_2",                  "[0.1.2]"},
        {"n_number_0_dot_3e_plus",                  "[0.3e+]"},
        {"n_number_0_dot_3e",                       "[0.3e]"},
        {"n_number_0_dot_e1",                       "[0.e1]"},
        {"n_number_0E_plus",                        "[0E+]"},
        {"n_number_0e",                             "[0e]"},
        {"n_number_1_dot_0e_plus",                  "[1.0e+]"},
        {"n_number_1_dot_0e_minus",                 "[1.0e-]"},
        {"n_number_1_dot_0e",                       "[1.0e]"},
        {"n_number_1eE2",                           "[1eE2]"},
        {"n_number_Inf",                            "[Inf]"},
        {"n_number_NaN",                            "[NaN]"},
        {"n_number_infinity",                       "[Infinity]"},
        {"n_number_minus_infinity",                 "[-Infinity]"},
        {"n_number_expression",                     "[1+2]"},
        {"n_number_hex_1_digit",                    "[0x1]"},
        {"n_number_hex_2_digits",                   "[0x42]"},
        {"n_number_with_leading_zero",              "[012]"},
        {"n_number_real_without_frac_part",         "[1.]"},
        {"n_number_starting_with_dot",              "[.123]"},
        {"n_number_neg_real_without_int",           "[-.123]"},
        {"n_number_minus_space_1",                  "[- 1]"},
        {"n_number_1_000",                          "[1 000]"},

        // ---- strings ----
        {"n_string_escape_x",                       "[\"\\x00\"]"},
        {"n_string_invalid_backslash_esc",          "[\"\\a\"]"},
        {"n_string_single_quote",                   "['single quote']"},
        {"n_string_single_doublequote",             "\""},
        {"n_string_single_string_no_double_quotes", "abc"},
        {"n_string_unicode_CapitalU",               "[\"\\UA66D\"]"},
        {"n_string_incomplete_escape",              "[\"\\\"]"},
        {"n_string_incomplete_unicode",             "[\"\\u00A\"]"},
        {"n_string_invalid_unicode_escape",         "[\"\\uqqqq\"]"},
        {"n_string_incomplete_surrogate",           "[\"\\uD834\\uDd\"]"},
        {"n_string_unescaped_newline",              std::string("[\"new\nline\"]")},
        {"n_string_unescaped_tab",                  std::string("[\"a\tb\"]")},

        // ---- objects ----
        {"n_object_non_string_key",                 "{1:1}"},
        {"n_object_missing_colon",                  "{\"a\" b}"},
        {"n_object_missing_value",                  "{\"a\":}"},
        {"n_object_missing_key",                    "{:\"b\"}"},
        {"n_object_double_colon",                   "{\"x\"::\"b\"}"},
        {"n_object_trailing_comma",                 "{\"id\":0,}"},
        {"n_object_several_trailing_commas",        "{\"id\":0,,,,,}"},
        {"n_object_two_commas_in_a_row",            "{\"a\":\"b\",,\"c\":\"d\"}"},
        {"n_object_unquoted_key",                   "{a: \"b\"}"},
        {"n_object_single_quote",                   "{'a':0}"},
        {"n_object_comma_instead_of_colon",         "{\"x\", \"x\"}"},
        {"n_object_bracket_key",                    "{[: \"x\"}"},
        {"n_object_with_single_string",             "{ \"foo\" : \"bar\", \"a\" }"},
        {"n_object_repeated_null_null",             "{null:null,null:null}"},

        // ---- whitespace edge cases ----
        {"n_whitespace_formfeed",                   std::string("[\f]")},
        {"n_whitespace_word_joiner",                std::string("\xE2\x81\xA0{}")},    // U+2060

        // ---- comments in strict mode ----
        {"n_structure_object_with_comment",         "{\"a\":/*comment*/\"b\"}"},
        {"n_structure_trailing_comment",            "{\"a\":\"b\"}/**/"},
        {"n_structure_trailing_comment_slash",       "{\"a\":\"b\"}//"},
    };

    for (const auto& tc : cases) {
        EXPECT_TRUE(must_reject(tc.input))
            << "FAIL n_: " << tc.name << "  input: " << tc.input;
    }
}

// ========================================================================
//  Extension mode: comments must be accepted (not strict RFC)
// ========================================================================
TEST(JSONTestSuite, extension_mode_comments_accepted) {
    EXPECT_TRUE(must_accept_ext("{\"a\":/*comment*/\"b\"}"));
    EXPECT_TRUE(must_accept_ext("{\"a\":\"b\"}/**/"));
    EXPECT_TRUE(must_accept_ext("[1]//tail\n"));
    EXPECT_TRUE(must_accept_ext("/**/42"));
}

// ========================================================================
//  UTF-8 validation mode (ParseJsonStrictUtf8)
// ========================================================================
TEST(JSONTestSuite, utf8_validation_rejects_invalid_bytes) {
    std::string err;

    // Valid UTF-8 should pass
    Json j1 = Json::ParseJsonStrictUtf8("[\"abc\"]", err);
    EXPECT_FALSE(j1.isError()) << err;

    Json j2 = Json::ParseJsonStrictUtf8("[\"\xc3\xa9\"]", err);  // é
    EXPECT_FALSE(j2.isError()) << err;

    Json j3 = Json::ParseJsonStrictUtf8("[\"\xe4\xbd\xa0\xe5\xa5\xbd\"]", err);  // 你好
    EXPECT_FALSE(j3.isError()) << err;

    Json j4 = Json::ParseJsonStrictUtf8("[\"\xf0\x9d\x84\x9e\"]", err);  // U+1D11E
    EXPECT_FALSE(j4.isError()) << err;

    // Invalid: lone continuation byte
    Json j5 = Json::ParseJsonStrictUtf8(std::string("[\"a\x80\"]"), err);
    EXPECT_TRUE(j5.isError());
    EXPECT_NE(err.find("UTF-8"), std::string::npos);

    // Invalid: overlong 2-byte encoding of ASCII
    Json j6 = Json::ParseJsonStrictUtf8(std::string("[\"a\xc0\xaf\"]"), err);
    EXPECT_TRUE(j6.isError());

    // Invalid: truncated 3-byte sequence
    Json j7 = Json::ParseJsonStrictUtf8(std::string("[\"a\xe0\x80\"]"), err);
    EXPECT_TRUE(j7.isError());

    // Invalid: surrogate half U+D800
    Json j8 = Json::ParseJsonStrictUtf8(std::string("[\"a\xed\xa0\x80\"]"), err);
    EXPECT_TRUE(j8.isError());

    // Invalid: above U+10FFFF  (F4 90 80 80)
    Json j9 = Json::ParseJsonStrictUtf8(std::string("[\"a\xf4\x90\x80\x80\"]"), err);
    EXPECT_TRUE(j9.isError());

    // Invalid: 5-byte sequence (never valid UTF-8)
    Json j10 = Json::ParseJsonStrictUtf8(std::string("[\"a\xf8\x80\x80\x80\x80\"]"), err);
    EXPECT_TRUE(j10.isError());
}

TEST(JSONTestSuite, utf8_validation_accepts_valid_in_keys) {
    std::string err;
    // UTF-8 in key names
    Json j = Json::ParseJsonStrictUtf8("{\"cl\xc3\xa9\":\"val\"}", err);
    EXPECT_FALSE(j.isError()) << err;
}

// ========================================================================
//  Deep nesting (PDA must handle without stack overflow)
// ========================================================================
TEST(JSONTestSuite, deep_nesting_pda_safety) {
    // 100 levels should succeed (max_depth)
    {
        std::string deep;
        for (int i = 0; i < 100; ++i) deep.push_back('[');
        deep += "null";
        for (int i = 0; i < 100; ++i) deep.push_back(']');

        std::string err;
        Json j = Json::ParseJsonStrict(deep, err);
        EXPECT_FALSE(j.isError()) << err;
    }

    // 102 levels should fail (> max_depth)
    {
        std::string deep;
        for (int i = 0; i < 102; ++i) deep.push_back('[');
        deep += "null";
        for (int i = 0; i < 102; ++i) deep.push_back(']');

        std::string err;
        Json j = Json::ParseJsonStrict(deep, err);
        EXPECT_TRUE(j.isError());
    }

    // Even very deep failing nesting must not crash (PDA uses heap stack)
    {
        std::string deep;
        for (int i = 0; i < 10000; ++i) deep.push_back('[');
        std::string err;
        Json j = Json::ParseJsonStrict(deep, err);
        EXPECT_TRUE(j.isError());
    }
}

// ========================================================================
//  Stress: parse → copy → move → toString → re-parse in tight loop
// ========================================================================
TEST(JSONTestSuite, stress_round_trip) {
    const std::string sample =
        "{\"arr\":[1,2,3],\"obj\":{\"key\":\"value\",\"n\":-0.5e+3},\"t\":true,\"f\":false,\"nil\":null}";

    for (int i = 0; i < 3000; ++i) {
        std::string err;
        Json parsed = Json::ParseJsonStrict(sample, err);
        ASSERT_FALSE(parsed.isError()) << err;

        Json copied = parsed;
        Json moved = std::move(copied);

        std::string out = moved.toString();
        Json reparsed = Json::ParseJsonStrict(out, err);
        ASSERT_FALSE(reparsed.isError()) << err;
        ASSERT_EQ(reparsed["arr"][1].toInt(), 2);
    }
}
