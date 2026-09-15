// =============================================================================
//  ZJSON :: parser coverage suite
//
//  Companion to tests/test_api_coverage.cpp.  This file covers the parsing
//  surface end to end:
//    - the four static entry points and ParseOptions combinations
//    - extension (comment) mode vs strict RFC 8259 mode
//    - duplicate-key policies
//    - number / string / literal grammar, including every rejection path
//    - nesting-depth boundary
//    - byte-level UTF-8 validation
//    - error reporting (line/column, message text)
//    - file loading
//    - large-document stress round trips
// =============================================================================
#include "gtest/gtest.h"
#include "../src/zjson.hpp"

#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

using namespace ZJSON;

namespace {

bool accepts(const std::string& text) {
	std::string err;
	Json parsed = Json::ParseJson(text, err);
	return !parsed.isError();
}

bool acceptsStrict(const std::string& text, ParseOptions options = ParseOptions{}) {
	options.allowComments = false;
	std::string err;
	Json parsed = Json::ParseJson(text, err, options);
	return !parsed.isError();
}

bool acceptsUtf8(const std::string& text) {
	std::string err;
	Json parsed = Json::ParseJsonStrictUtf8(text, err);
	return !parsed.isError();
}

Json parseStrict(const std::string& text) {
	std::string err;
	Json parsed = Json::ParseJsonStrict(text, err);
	EXPECT_FALSE(parsed.isError()) << "unexpected parse failure for: " << text << " -> " << err;
	return parsed;
}

std::string errorOf(const std::string& text, bool strict = true) {
	std::string err;
	if (strict)
		Json::ParseJsonStrict(text, err);
	else
		Json::ParseJson(text, err);
	return err;
}

} // namespace

// -----------------------------------------------------------------------------
// Entry points
// -----------------------------------------------------------------------------

TEST(TestParseCoverage, entry_point_matrix) {
	const std::string text = "{\"a\":[1,2]}";
	std::string err;

	Json lenient = Json::ParseJson(text, err);
	EXPECT_FALSE(lenient.isError());
	EXPECT_TRUE(err.empty());

	err.clear();
	ParseOptions options;
	Json withOptions = Json::ParseJson(text, err, options);
	EXPECT_FALSE(withOptions.isError());

	err.clear();
	Json strict = Json::ParseJsonStrict(text, err);
	EXPECT_FALSE(strict.isError());

	err.clear();
	Json strictWithOptions = Json::ParseJsonStrict(text, err, options);
	EXPECT_FALSE(strictWithOptions.isError());

	err.clear();
	Json strictUtf8 = Json::ParseJsonStrictUtf8(text, err);
	EXPECT_FALSE(strictUtf8.isError());

	err.clear();
	Json strictUtf8WithOptions = Json::ParseJsonStrictUtf8(text, err, options);
	EXPECT_FALSE(strictUtf8WithOptions.isError());

	// All entry points agree on a valid document.
	EXPECT_EQ(lenient, strict);
	EXPECT_EQ(lenient, strictUtf8);

	// Option struct defaults are part of the contract.
	EXPECT_TRUE(ParseOptions{}.allowComments);
	EXPECT_FALSE(ParseOptions{}.validateUtf8);
	EXPECT_TRUE(ParseOptions{}.duplicateKey == ParseOptions::DuplicateKeyPolicy::KeepLast);

	// Passing allowComments=false to the lenient entry point behaves strictly.
	ParseOptions strictOptions;
	strictOptions.allowComments = false;
	err.clear();
	EXPECT_TRUE(Json::ParseJson("{\"a\":1/*c*/}", err, strictOptions).isError());

	// Passing validateUtf8=true to the lenient entry point validates bytes.
	ParseOptions utf8Options;
	utf8Options.validateUtf8 = true;
	err.clear();
	EXPECT_TRUE(Json::ParseJson(std::string("\"\x80\""), err, utf8Options).isError());
}

TEST(TestParseCoverage, scalar_documents_and_whitespace_tolerance) {
	std::string err;
	EXPECT_TRUE(Json::ParseJsonStrict("null", err).isNull());
	EXPECT_TRUE(parseStrict("true").isTrue());
	EXPECT_TRUE(parseStrict("false").isFalse());
	EXPECT_EQ(parseStrict("0").toInt(), 0);
	EXPECT_EQ(parseStrict("\"text\"").toString(), "text");
	EXPECT_EQ(parseStrict("[]").toString(), "[]");
	EXPECT_EQ(parseStrict("{}").toString(), "{}");

	// All four JSON whitespace characters are accepted around every token.
	EXPECT_TRUE(acceptsStrict(" \t\r\n [\n1\t,\r2\n] \t\r\n "));
	EXPECT_TRUE(acceptsStrict("\t{\r\"a\"\n:\t1\r}\n"));
	EXPECT_EQ(parseStrict(" [ 1 , 2 ] ").size(), 2);

	// Empty and whitespace-only inputs are errors.
	EXPECT_FALSE(acceptsStrict(""));
	EXPECT_FALSE(acceptsStrict(" "));
	EXPECT_FALSE(acceptsStrict("\n\t\r "));
	EXPECT_FALSE(accepts(""));

	// Non-whitespace control characters outside strings are not tolerated.
	EXPECT_FALSE(acceptsStrict(std::string("[\f1]")));
	EXPECT_FALSE(acceptsStrict(std::string("\x01[1]")));
}

// -----------------------------------------------------------------------------
// Comments (extension mode)
// -----------------------------------------------------------------------------

TEST(TestParseCoverage, comment_forms_in_extension_mode) {
	const std::vector<std::string> commented = {
		"// leading line comment\n{\"a\":1}",
		"/* leading block comment */{\"a\":1}",
		"{\"a\":1}// trailing line comment",
		"{\"a\":1}/* trailing block */",
		"{\"a\":1,/* between members */\"b\":2}",
		"[1,/*x*/2,3]",
		"[/*a*/1/*b*/,/*c*/2/*d*/]",
		"{/*a*/\"k\"/*b*/:/*c*/1/*d*/}",
		"/**//**/{\"a\":1}/**/",
		"/* multi\n   line\n   comment */{\"a\":1}"
	};

	for (const std::string& text : commented) {
		EXPECT_TRUE(accepts(text)) << "should accept: " << text;
		EXPECT_FALSE(acceptsStrict(text)) << "strict mode must reject: " << text;
	}

	// Comments are stripped, not stored.
	EXPECT_EQ(parseStrict("{\"a\":1}").toString(), "{\"a\":1}");
	std::string err;
	Json withComment = Json::ParseJson("/*c*/{\"a\":1}//c", err);
	ASSERT_FALSE(withComment.isError()) << err;
	EXPECT_EQ(withComment.toString(), "{\"a\":1}");
}

TEST(TestParseCoverage, comment_error_cases) {
	EXPECT_FALSE(accepts("/*"));
	EXPECT_FALSE(accepts("/* only a comment"));
	EXPECT_FALSE(accepts("{\"a\":1}/* unterminated"));
	EXPECT_FALSE(accepts("/"));
	EXPECT_FALSE(accepts("/x"));
	EXPECT_FALSE(accepts("[1,/x]"));
	EXPECT_FALSE(accepts("//"));
	EXPECT_FALSE(accepts("// only a comment"));
	EXPECT_FALSE(accepts("/**/"));

	EXPECT_NE(errorOf("/*", false).find("comment"), std::string::npos);
	EXPECT_NE(errorOf("/x", false).find("malformed comment"), std::string::npos);
	EXPECT_NE(errorOf("/", false).find("unexpected end of input"), std::string::npos);

	// A comment object with a valid tail is still fine.
	EXPECT_TRUE(accepts("/*a*/{\"b\":1}/*c*/"));
}

// -----------------------------------------------------------------------------
// Duplicate keys
// -----------------------------------------------------------------------------

TEST(TestParseCoverage, duplicate_key_policies) {
	const std::string input = "{\"a\":1,\"b\":9,\"a\":2,\"a\":3}";

	std::string err;
	Json keepLast = Json::ParseJsonStrict(input, err);
	ASSERT_FALSE(keepLast.isError()) << err;
	EXPECT_EQ(keepLast["a"].toInt(), 3);
	EXPECT_EQ(keepLast.toString(), "{\"b\":9,\"a\":3}");

	ParseOptions keepFirstOptions;
	keepFirstOptions.duplicateKey = ParseOptions::DuplicateKeyPolicy::KeepFirst;
	err.clear();
	Json keepFirst = Json::ParseJsonStrict(input, err, keepFirstOptions);
	ASSERT_FALSE(keepFirst.isError()) << err;
	EXPECT_EQ(keepFirst["a"].toInt(), 1);
	EXPECT_EQ(keepFirst.toString(), "{\"a\":1,\"b\":9}");

	ParseOptions rejectOptions;
	rejectOptions.duplicateKey = ParseOptions::DuplicateKeyPolicy::Reject;
	err.clear();
	Json rejected = Json::ParseJsonStrict(input, err, rejectOptions);
	EXPECT_TRUE(rejected.isError());
	EXPECT_NE(err.find("duplicate key 'a'"), std::string::npos);

	// The policy applies independently to every object in the document.
	const std::string nested = "{\"o\":{\"k\":1,\"k\":2},\"k\":1,\"k\":2}";
	err.clear();
	Json nestedLast = Json::ParseJsonStrict(nested, err);
	ASSERT_FALSE(nestedLast.isError()) << err;
	EXPECT_EQ(nestedLast.toString(), "{\"o\":{\"k\":2},\"k\":2}");

	err.clear();
	Json nestedFirst = Json::ParseJsonStrict(nested, err, keepFirstOptions);
	ASSERT_FALSE(nestedFirst.isError()) << err;
	EXPECT_EQ(nestedFirst.toString(), "{\"o\":{\"k\":1},\"k\":1}");

	err.clear();
	EXPECT_TRUE(Json::ParseJsonStrict(nested, err, rejectOptions).isError());

	// Duplicates whose values are containers keep the surviving subtree intact.
	const std::string containers = "{\"a\":{\"x\":1},\"a\":[1,2]}";
	err.clear();
	Json containerLast = Json::ParseJsonStrict(containers, err);
	ASSERT_FALSE(containerLast.isError()) << err;
	EXPECT_EQ(containerLast.toString(), "{\"a\":[1,2]}");

	err.clear();
	Json containerFirst = Json::ParseJsonStrict(containers, err, keepFirstOptions);
	ASSERT_FALSE(containerFirst.isError()) << err;
	EXPECT_EQ(containerFirst.toString(), "{\"a\":{\"x\":1}}");

	// Many duplicates in a wide object exercise the small-index/hash upgrade.
	std::string wide = "{";
	for (int i = 0; i < 40; ++i) {
		if (i) wide += ',';
		wide += "\"k" + std::to_string(i) + "\":" + std::to_string(i);
	}
	wide += ",\"k0\":999}";
	err.clear();
	Json wideParsed = Json::ParseJsonStrict(wide, err);
	ASSERT_FALSE(wideParsed.isError()) << err;
	EXPECT_EQ(wideParsed["k0"].toInt(), 999);
	EXPECT_EQ(wideParsed["k39"].toInt(), 39);
}

// -----------------------------------------------------------------------------
// Numbers
// -----------------------------------------------------------------------------

TEST(TestParseCoverage, valid_number_forms) {
	const std::vector<std::pair<std::string, double>> cases = {
		{ "0", 0.0 }, { "-0", 0.0 }, { "1", 1.0 }, { "-1", -1.0 },
		{ "123456789", 123456789.0 }, { "0.5", 0.5 }, { "-0.5", -0.5 },
		{ "1.25", 1.25 }, { "1e2", 100.0 }, { "1E2", 100.0 },
		{ "1e+2", 100.0 }, { "1e-2", 0.01 }, { "0e0", 0.0 },
		{ "1.5e3", 1500.0 }, { "1.5E-3", 0.0015 }, { "-1.5e-3", -0.0015 },
		{ "0.0000009999", 0.0000009999 }
	};

	for (const auto& [text, expected] : cases) {
		Json parsed = parseStrict(text);
		ASSERT_TRUE(parsed.isNumber()) << text;
		EXPECT_DOUBLE_EQ(parsed.toDouble(), expected) << text;
		EXPECT_FALSE(parsed.toString().empty()) << text;
	}

	// Non-finite magnitudes are accepted at parse time (implementation-defined)
	// and degrade to null when serialized, which is the documented behaviour.
	EXPECT_TRUE(acceptsStrict("1e999"));
	EXPECT_TRUE(acceptsStrict("-1e+9999"));
	EXPECT_TRUE(acceptsStrict("1.5e9999"));
	EXPECT_TRUE(acceptsStrict("5e-324"));
	EXPECT_TRUE(acceptsStrict("1e-999"));
	EXPECT_EQ(parseStrict("1e999").toString(), "null");

	// Negative zero keeps its sign through both directions.
	EXPECT_EQ(parseStrict("-0").toString(), "-0");
	EXPECT_EQ(parseStrict("-0.0").toString(), "-0");

	// Integer magnitudes beyond 2^53 are accepted but stored as double: the
	// documented precision limit of the current numeric model.
	EXPECT_TRUE(acceptsStrict("9007199254740993"));
	EXPECT_TRUE(acceptsStrict("9223372036854775807"));
	EXPECT_TRUE(acceptsStrict("-9223372036854775808"));
	EXPECT_EQ(parseStrict("9007199254740993").toString(), "9007199254740992");
}

TEST(TestParseCoverage, invalid_number_forms) {
	const std::vector<std::string> invalids = {
		"01", "-01", "007",
		"+1", "-", "--1", "+", ".",
		".5", "1.", "1.e2", "1.2.3",
		"1e", "1e+", "1e-", "1eE2", "1e2.5",
		"0x1", "0b1", "1_000",
		"NaN", "Infinity", "-Infinity", "inf", "nan",
		"1a", "0xab", "1.", "[1]x"
	};

	for (const std::string& text : invalids) {
		EXPECT_FALSE(acceptsStrict(text)) << "must reject: " << text;
	}

	// Numbers are not accepted inside a bare container context either.
	EXPECT_FALSE(acceptsStrict("[01]"));
	EXPECT_FALSE(acceptsStrict("[1.]"));
	EXPECT_FALSE(acceptsStrict("[-]"));
	EXPECT_FALSE(acceptsStrict("[1e+]"));

	// "1 2" is two values, not one.
	EXPECT_FALSE(acceptsStrict("1 2"));
	EXPECT_NE(errorOf("01").find("leading 0s"), std::string::npos);
	EXPECT_NE(errorOf("1e").find("exponent"), std::string::npos);
	EXPECT_NE(errorOf("1.").find("fractional"), std::string::npos);
}

// -----------------------------------------------------------------------------
// Strings
// -----------------------------------------------------------------------------

TEST(TestParseCoverage, string_escapes_and_unicode) {
	Json parsed = parseStrict(
		"{\"esc\":\"\\\\ \\\" \\/ \\b \\f \\n \\r \\t\","
		"\"unicode\":\"\\u4F60\\u597D\","
		"\"pair\":\"\\uD83D\\uDE00\","
		"\"ascii\":\"plain\"}");

	EXPECT_EQ(parsed["esc"].toString(), "\\ \" / \b \f \n \r \t");
	EXPECT_EQ(parsed["unicode"].toString(), "\xE4\xBD\xA0\xE5\xA5\xBD");         // 你好
	EXPECT_EQ(parsed["pair"].toString(), "\xF0\x9F\x98\x80");                     // U+1F600
	EXPECT_EQ(parsed["ascii"].toString(), "plain");

	// Escaped text survives a full round trip.
	EXPECT_EQ(Json(parsed.toString()), parsed);

	// Empty strings and empty keys are legal.
	EXPECT_EQ(parseStrict("\"\"").toString(), "");
	EXPECT_EQ(parseStrict("{\"\":\"\"}").toString(), "{\"\":\"\"}");
	EXPECT_EQ(parseStrict("{\"\":1}").at("/").toInt(), 1);

	// A string may contain any non-control byte, including raw UTF-8.
	EXPECT_EQ(parseStrict("\"\xE4\xB8\xAD\xE6\x96\x87\"").toString(), "\xE4\xB8\xAD\xE6\x96\x87");
}

TEST(TestParseCoverage, invalid_string_forms) {
	const std::vector<std::string> invalids = {
		"\"unterminated",
		"\"bad\\x00\"",
		"\"bad\\u12G4\"",
		"\"bad\\u123\"",
		"\"bad\\q\"",
		"\"bad\\\"",
		"\"tab\tinside\"",             // raw tab
		"\"line\ninside\"",            // raw newline
		"\"cr\rinside\"",
		"\"back\bspace\"",
		"{\"key\n\":1}",
		"{\"unterminated:1}",
		"[\"a\",]"
	};

	for (const std::string& text : invalids) {
		EXPECT_FALSE(acceptsStrict(text)) << "must reject: " << text;
	}

	// Raw control characters must be escaped.
	std::string withRawControl = "\"a";
	withRawControl.push_back('\x1f');
	withRawControl += "b\"";
	EXPECT_FALSE(acceptsStrict(withRawControl));

	// Escaped control characters are fine.
	EXPECT_TRUE(acceptsStrict("\"a\\u001fb\""));
	EXPECT_EQ(parseStrict("\"a\\u001fb\"").toString(), std::string("a\x1f" "b", 3));
}

// -----------------------------------------------------------------------------
// Literals and structure
// -----------------------------------------------------------------------------

TEST(TestParseCoverage, literal_and_structure_errors) {
	const std::vector<std::string> invalids = {
		"tru", "truex", "True", "TRUE",
		"fal", "falsex", "False",
		"nul", "nullx", "Null", "NULL", "None",
		"undefined",
		"[tru]", "[nul]", "[True]",
		"{\"a\":tru}", "{\"a\":nul}"
	};

	for (const std::string& text : invalids) {
		EXPECT_FALSE(acceptsStrict(text)) << "must reject: " << text;
	}

	const std::vector<std::string> structural = {
		"[1,]", "{\"a\":1,}", "{\"a\" 1}", "{\"a\":}", "{key:1}",
		"{'a':1}", "[1 2]", "{\"a\":1 \"b\":2}", "[,1]", "[1,,2]",
		"[", "]", "{", "}", "{\"a\":1", "[1,2", "{\"a\"", "{\"a\":",
		"[]extra", "{}extra", "{\"a\":1}x", "[1]x", "true false",
		"{\"a\":1}{\"b\":2}", "[1][2]", "nulll"
	};

	for (const std::string& text : structural) {
		EXPECT_FALSE(acceptsStrict(text)) << "must reject: " << text;
	}

	EXPECT_NE(errorOf("{\"a\" 1}").find("expected ':'"), std::string::npos);
	EXPECT_NE(errorOf("{\"a\":1}x").find("unexpected trailing"), std::string::npos);
	EXPECT_NE(errorOf("[1 2]").find("expected ','"), std::string::npos);
}

TEST(TestParseCoverage, nesting_depth_boundary) {
	auto nest = [](int depth) {
		std::string text;
		for (int i = 0; i < depth; ++i) text.push_back('[');
		for (int i = 0; i < depth; ++i) text.push_back(']');
		return text;
	};

	EXPECT_TRUE(acceptsStrict(nest(1)));
	EXPECT_TRUE(acceptsStrict(nest(50)));
	EXPECT_TRUE(acceptsStrict(nest(100)));
	EXPECT_TRUE(acceptsStrict(nest(101)));          // highest accepted level
	EXPECT_FALSE(acceptsStrict(nest(102)));         // first rejected level
	EXPECT_FALSE(acceptsStrict(nest(200)));
	EXPECT_NE(errorOf(nest(102)).find("exceeded maximum nesting depth"), std::string::npos);

	// Objects count towards the same limit.
	std::string objects;
	for (int i = 0; i < 102; ++i) objects += "{\"k\":";
	for (int i = 0; i < 102; ++i) objects += "}";
	EXPECT_FALSE(acceptsStrict(objects));
}

// -----------------------------------------------------------------------------
// UTF-8 validation
// -----------------------------------------------------------------------------

TEST(TestParseCoverage, utf8_validation_accepts_valid_sequences) {
	const std::vector<std::string> valids = {
		"\"ascii\"",
		"\"\xC2\xA9\"",                 // U+00A9 (2 bytes)
		"\"\xE2\x82\xAC\"",             // U+20AC (3 bytes)
		"\"\xF0\x9D\x84\x9E\"",         // U+1D11E (4 bytes)
		"\"\xE4\xB8\xAD\xE6\x96\x87\"", // 中文
		"{\"\xE4\xB8\xAD\":\"\xE6\x96\x87\"}",
		"[]"
	};

	for (const std::string& text : valids) {
		EXPECT_TRUE(acceptsUtf8(text)) << "should accept: " << text;
	}

	// The validated document keeps its bytes.
	std::string err;
	Json parsed = Json::ParseJsonStrictUtf8("\"\xE4\xB8\xAD\xE6\x96\x87\"", err);
	ASSERT_FALSE(parsed.isError()) << err;
	EXPECT_EQ(parsed.toString(), "\xE4\xB8\xAD\xE6\x96\x87");
}

TEST(TestParseCoverage, utf8_validation_rejects_invalid_sequences) {
	const std::vector<std::string> invalids = {
		std::string("\"\x80\""),                 // stray continuation byte
		std::string("\"\xBF\""),
		std::string("\"\xC0\x80\""),             // overlong 2-byte encoding of NUL
		std::string("\"\xC1\xBF\""),             // overlong
		std::string("\"\xE0\x80\x80\""),         // overlong 3-byte
		std::string("\"\xED\xA0\x80\""),         // UTF-16 surrogate half
		std::string("\"\xED\xBF\xBF\""),
		std::string("\"\xF0\x80\x80\x80\""),     // overlong 4-byte
		std::string("\"\xF4\x90\x80\x80\""),     // above U+10FFFF
		std::string("\"\xF5\x80\x80\x80\""),     // invalid lead byte
		std::string("\"\xFF\""),
		std::string("\"\xC3\""),                 // truncated sequence
		std::string("\"\xE2\x82\""),
		std::string("{\"\x80\":1}"),             // invalid bytes inside a key
		std::string("\"\xE4\xB8\xAD\x80\"")      // valid text followed by a stray byte
	};

	for (const std::string& text : invalids) {
		EXPECT_FALSE(acceptsUtf8(text)) << "must reject: " << text;
	}

	// The error message points at the offending byte position.
	std::string err;
	EXPECT_TRUE(Json::ParseJsonStrictUtf8(std::string("\"a\x80" "b\""), err).isError());
	EXPECT_NE(err.find("invalid UTF-8 byte at position"), std::string::npos);

	// Validation is opt-in: the byte-wise entry points still accept these bytes,
	// and the strict entry point does too (it validates structure, not encoding).
	EXPECT_TRUE(acceptsStrict(std::string("\"\x80\"")));
	EXPECT_TRUE(accepts(std::string("\"\x80\"")));
}

// -----------------------------------------------------------------------------
// Error reporting
// -----------------------------------------------------------------------------

TEST(TestParseCoverage, error_messages_report_line_and_column) {
	auto matchesPosition = [](const std::string& err) {
		return err.find("line ") != std::string::npos && err.find(", col ") != std::string::npos;
	};

	EXPECT_TRUE(matchesPosition(errorOf("{\"a\" 1}")));
	EXPECT_TRUE(matchesPosition(errorOf("{\n\"a\":}")));
	EXPECT_TRUE(matchesPosition(errorOf("[\n1,\n]")));
	EXPECT_TRUE(matchesPosition(errorOf("[1,2")));

	// Line and column are 1-based and follow the input.
	EXPECT_NE(errorOf("{\n\"a\":}").find("line 2"), std::string::npos);
	EXPECT_NE(errorOf("{\n\n\n\"a\" 1}").find("line 4"), std::string::npos);

	// Error nodes carry the message in their name as well.
	std::string err;
	Json failed = Json::ParseJsonStrict("{[}", err);
	ASSERT_TRUE(failed.isError());
	EXPECT_FALSE(err.empty());

	// Success leaves the error string empty.
	err = "stale";
	Json ok = Json::ParseJsonStrict("{}", err);
	EXPECT_FALSE(ok.isError());
	EXPECT_TRUE(err.empty());
}

// -----------------------------------------------------------------------------
// File loading
// -----------------------------------------------------------------------------

TEST(TestParseCoverage, from_file_cases) {
	const std::string path = "zjson_from_file_test.json";

	{
		std::ofstream out(path, std::ios::binary);
		ASSERT_TRUE(out.is_open());
		out << "{\"a\":[1,2,3],\"b\":{\"c\":\"text\"}}";
	}
	Json loaded = Json::FromFile(path);
	ASSERT_FALSE(loaded.isError());
	EXPECT_EQ(loaded["a"][2].toInt(), 3);
	EXPECT_EQ(loaded["b"]["c"].toString(), "text");
	EXPECT_EQ(Json::FromFile(std::string(path)), loaded);

	// Pretty-printed and whitespace-padded files load as well.
	{
		std::ofstream out(path, std::ios::binary);
		out << "\n  {\n    \"nested\": [true, null]\n  }\n";
	}
	Json padded = Json::FromFile(path);
	ASSERT_FALSE(padded.isError());
	EXPECT_TRUE(padded["nested"][0].isTrue());
	EXPECT_TRUE(padded["nested"][1].isNull());

	// An empty file is an error.
	{
		std::ofstream out(path, std::ios::binary);
		out << "";
	}
	EXPECT_TRUE(Json::FromFile(path).isError());

	// A whitespace-only file is not JSON: it falls back to a string value.
	{
		std::ofstream out(path, std::ios::binary);
		out << "   ";
	}
	EXPECT_TRUE(Json::FromFile(path).isString());

	// A file whose content looks like a document but is not valid JSON is reported as an
	// error: FromFile hands document-shaped input to the parser (which also avoids a
	// second copy of the text), so a broken configuration file fails loudly instead of
	// being silently stored as a string. The string constructor keeps its historical
	// fallback (see lenient_constructor_fallback_rules).
	{
		std::ofstream out(path, std::ios::binary);
		out << "{\"broken\": }";
	}
	EXPECT_TRUE(Json::FromFile(path).isError());

	// A file that is not document-shaped keeps the historical behaviour of becoming a
	// string value holding the raw text (quotes included - the string constructor is a
	// "text" fallback, not a JSON string parser).
	{
		std::ofstream out(path, std::ios::binary);
		out << "\"just text\"";
	}
	EXPECT_TRUE(Json::FromFile(path).isString());
	EXPECT_EQ(Json::FromFile(path).toString(), "\"just text\"");

	// Missing paths and empty path names are errors.
	EXPECT_TRUE(Json::FromFile("definitely/missing/zjson.json").isError());
	EXPECT_TRUE(Json::FromFile(std::string("definitely/missing/zjson.json")).isError());
	EXPECT_TRUE(Json::FromFile("").isError());

	std::remove(path.c_str());
}

// -----------------------------------------------------------------------------
// Resilience of the lenient entry point
// -----------------------------------------------------------------------------

TEST(TestParseCoverage, lenient_constructor_fallback_rules) {
	// Text that starts like a container but is invalid becomes a string value.
	const std::vector<std::string> fallbacks = {
		"{", "}", "[", "]", "{\"a\":}", "[1,]", "[01]", "{\"a\" 1}", "{bad", "[bad"
	};
	for (const std::string& text : fallbacks) {
		Json value(text);
		EXPECT_TRUE(value.isString()) << "should fall back to string: " << text;
		EXPECT_EQ(value.toString(), text);
	}

	// Text that does not start like a container is a string from the start.
	EXPECT_TRUE(Json("plain text").isString());
	EXPECT_TRUE(Json("42").isString());          // deliberately not a number
	EXPECT_TRUE(Json("null").isString());
	EXPECT_TRUE(Json("true").isString());
	EXPECT_TRUE(Json("\"quoted\"").isString());
	EXPECT_EQ(Json("\"quoted\"").toString(), "\"quoted\"");

	// The strict entry point rejects what the constructor falls back on.
	EXPECT_FALSE(acceptsStrict("{\"a\":}"));
	EXPECT_TRUE(acceptsStrict("{\"a\":1}"));
}

// -----------------------------------------------------------------------------
// Stress
// -----------------------------------------------------------------------------

TEST(TestParseCoverage, large_document_round_trip) {
	std::string text = "{\"items\":[";
	const int count = 5000;
	for (int i = 0; i < count; ++i) {
		if (i) text += ',';
		text += "{\"id\":" + std::to_string(i) + ",\"name\":\"item" + std::to_string(i) + "\",\"flag\":" + (i % 2 ? "true" : "false") + "}";
	}
	text += "]}";

	Json parsed = parseStrict(text);
	ASSERT_TRUE(parsed.isObject());
	ASSERT_EQ(parsed["items"].size(), count);
	EXPECT_EQ(parsed["items"][0]["id"].toInt(), 0);
	EXPECT_EQ(parsed["items"][count - 1]["name"].toString(), "item" + std::to_string(count - 1));
	EXPECT_TRUE(parsed["items"][1]["flag"].isTrue());
	EXPECT_FALSE(parsed["items"][0]["flag"].isTrue());

	// Serialization round-trips in both compact and pretty form.
	const std::string compact = parsed.toString();
	EXPECT_EQ(parseStrict(compact), parsed);
	EXPECT_EQ(Json(parsed.toString(2)), parsed);

	// Bulk mutation of a large document keeps it consistent. Note that
	// operator[] hands out copies, so the array is taken out of the document,
	// mutated, and put back - which is also the documented workaround.
	Json copy = parsed;
	Json items = copy.take("items");
	ASSERT_EQ(items.size(), count);
	EXPECT_EQ(items.take(0)["id"].toInt(), 0);
	EXPECT_EQ(items.size(), count - 1);
	items.remove(0);
	EXPECT_EQ(items[0]["id"].toInt(), 2);
	EXPECT_EQ(items.size(), count - 2);
	copy.add("items", std::move(items));
	copy.add("itemsCount", copy["items"].size());
	EXPECT_EQ(copy["itemsCount"].toInt(), count - 2);
	EXPECT_EQ(copy["items"].size(), count - 2);
}

TEST(TestParseCoverage, wide_object_round_trip) {
	std::string text = "{";
	const int count = 1000;
	for (int i = 0; i < count; ++i) {
		if (i) text += ',';
		text += "\"key" + std::to_string(i) + "\":" + std::to_string(i);
	}
	text += "}";

	Json parsed = parseStrict(text);
	EXPECT_EQ(parsed.getAllKeys().size(), count);
	EXPECT_EQ(parsed["key0"].toInt(), 0);
	EXPECT_EQ(parsed["key999"].toInt(), 999);
	EXPECT_TRUE(parsed.contains("key500"));
	EXPECT_FALSE(parsed.contains("key1000"));
	EXPECT_EQ(parsed.at("/key500").toInt(), 500);

	const std::string compact = parsed.toString();
	EXPECT_EQ(Json(compact), parsed);
	EXPECT_EQ(compact.size(), text.size());                      // compact form matches the source shape
	EXPECT_NE(parsed.toString(2).find("\"key999\": 999"), std::string::npos);
}
