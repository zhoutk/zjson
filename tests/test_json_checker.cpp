#include "gtest/gtest.h"
#include "../src/zjson.hpp"

#include <string>

using namespace ZJSON;

namespace {

constexpr int kJsonCheckerMaxDepth = 19;

struct JsonCheckerCase {
	const char* name;
	const char* content;
};

static const JsonCheckerCase kPassCases[] = {
	{"pass1.json", R"JSONCHK(
[
	"JSON Test Pattern pass1",
	{"object with 1 member":["array with 1 element"]},
	{},
	[],
	-42,
	true,
	false,
	null,
	{
		"integer": 1234567890,
		"real": -9876.543210,
		"e": 0.123456789e-12,
		"E": 1.234567890E+34,
		"":  23456789012E66,
		"zero": 0,
		"one": 1,
		"space": " ",
		"quote": "\"",
		"backslash": "\\",
		"controls": "\b\f\n\r\t",
		"slash": "/ & \/",
		"alpha": "abcdefghijklmnopqrstuvwyz",
		"ALPHA": "ABCDEFGHIJKLMNOPQRSTUVWYZ",
		"digit": "0123456789",
		"0123456789": "digit",
		"special": "`1~!@#$%^&*()_+-={':[,]}|;.</>?",
		"hex": "\u0123\u4567\u89AB\uCDEF\uabcd\uef4A",
		"true": true,
		"false": false,
		"null": null,
		"array":[  ],
		"object":{  },
		"address": "50 St. James Street",
		"url": "http://www.JSON.org/",
		"comment": "// /* <!-- --",
		"# -- --> */": " ",
		" s p a c e d " :[1,2 , 3

,

4 , 5        ,          6           ,7        ],"compact":[1,2,3,4,5,6,7],
		"jsontext": "{\"object with 1 member\":[\"array with 1 element\"]}",
		"quotes": "&#34; \u0022 %22 0x22 034 &#x22;",
		"\/\\\"\uCAFE\uBABE\uAB98\uFCDE\ubcda\uef4A\b\f\n\r\t`1~!@#$%^&*()_+-=[]{}|;:',./<>?"
: "A key can be any string"
	},
	0.5 ,98.6
,
99.44
,

1066,
1e1,
0.1e1,
1e-1,
1e00,2e+00,2e-00
,"rosebud"]
)JSONCHK"},
	{"pass2.json", R"JSONCHK(
[[[[[[[[[[[[[[[[[[["Not too deep"]]]]]]]]]]]]]]]]]]]
)JSONCHK"},
	{"pass3.json", R"JSONCHK(
{
	"JSON Test Pattern pass3": {
		"The outermost value": "must be an object or array.",
		"In this test": "It is an object."
	}
}
)JSONCHK"},
};

static const JsonCheckerCase kFailCases[] = {
	{"fail1.json", R"JSONCHK(
"A JSON payload should be an object or array, not a string."
)JSONCHK"},
	{"fail2.json", R"JSONCHK(
["Unclosed array"
)JSONCHK"},
	{"fail3.json", R"JSONCHK(
{unquoted_key: "keys must be quoted"}
)JSONCHK"},
	{"fail4.json", R"JSONCHK(
["extra comma",]
)JSONCHK"},
	{"fail5.json", R"JSONCHK(
["double extra comma",,]
)JSONCHK"},
	{"fail6.json", R"JSONCHK(
[   , "<-- missing value"]
)JSONCHK"},
	{"fail7.json", R"JSONCHK(
["Comma after the close"],
)JSONCHK"},
	{"fail8.json", R"JSONCHK(
["Extra close"]]
)JSONCHK"},
	{"fail9.json", R"JSONCHK(
{"Extra comma": true,}
)JSONCHK"},
	{"fail10.json", R"JSONCHK(
{"Extra value after close": true} "misplaced quoted value"
)JSONCHK"},
	{"fail11.json", R"JSONCHK(
{"Illegal expression": 1 + 2}
)JSONCHK"},
	{"fail12.json", R"JSONCHK(
{"Illegal invocation": alert()}
)JSONCHK"},
	{"fail13.json", R"JSONCHK(
{"Numbers cannot have leading zeroes": 013}
)JSONCHK"},
	{"fail14.json", R"JSONCHK(
{"Numbers cannot be hex": 0x14}
)JSONCHK"},
	{"fail15.json", R"JSONCHK(
["Illegal backslash escape: \x15"]
)JSONCHK"},
	{"fail16.json", R"JSONCHK(
[\naked]
)JSONCHK"},
	{"fail17.json", R"JSONCHK(
["Illegal backslash escape: \017"]
)JSONCHK"},
	{"fail18.json", R"JSONCHK(
[[[[[[[[[[[[[[[[[[[["Too deep"]]]]]]]]]]]]]]]]]]]]
)JSONCHK"},
	{"fail19.json", R"JSONCHK(
{"Missing colon" null}
)JSONCHK"},
	{"fail20.json", R"JSONCHK(
{"Double colon":: null}
)JSONCHK"},
	{"fail21.json", R"JSONCHK(
{"Comma instead of colon", null}
)JSONCHK"},
	{"fail22.json", R"JSONCHK(
["Colon instead of comma": false]
)JSONCHK"},
	{"fail23.json", R"JSONCHK(
["Bad value", truth]
)JSONCHK"},
	{"fail24.json", R"JSONCHK(
['single quote']
)JSONCHK"},
	{"fail25.json", "[\"\ttab\tcharacter\tin\tstring\t\"]"},
	{"fail26.json", R"JSONCHK(
["tab\   character\   in\  string\  "]
)JSONCHK"},
	{"fail27.json", R"JSONCHK(
["line
break"]
)JSONCHK"},
	{"fail28.json", R"JSONCHK(
["line\
break"]
)JSONCHK"},
	{"fail29.json", R"JSONCHK(
[0e]
)JSONCHK"},
	{"fail30.json", R"JSONCHK(
[0e+]
)JSONCHK"},
	{"fail31.json", R"JSONCHK(
[0e+-1]
)JSONCHK"},
	{"fail32.json", R"JSONCHK(
{"Comma instead if closing brace": true,
)JSONCHK"},
	{"fail33.json", R"JSONCHK(
["mismatch"}
)JSONCHK"},
};

bool respects_json_checker_depth_limit(const std::string& input) {
	int depth = 0;
	bool in_string = false;
	bool escaping = false;

	for (char ch : input) {
		if (in_string) {
			if (escaping) {
				escaping = false;
				continue;
			}
			if (ch == '\\') {
				escaping = true;
				continue;
			}
			if (ch == '"')
				in_string = false;
			continue;
		}

		if (ch == '"') {
			in_string = true;
			continue;
		}
		if (ch == '{' || ch == '[') {
			++depth;
			if (depth > kJsonCheckerMaxDepth)
				return false;
			continue;
		}
		if (ch == '}' || ch == ']')
			--depth;
	}

	return true;
}

bool parses_as_json_checker_document(const std::string& input) {
	if (!respects_json_checker_depth_limit(input))
		return false;

	std::string err;
	Json parsed = Json::ParseJsonStrict(input, err);
	return !parsed.isError() && (parsed.isObject() || parsed.isArray());
}

} // namespace

TEST(TestJsonChecker, accepts_all_pass_cases) {
	for (const auto& test_case : kPassCases)
		EXPECT_TRUE(parses_as_json_checker_document(test_case.content)) << test_case.name;
}

TEST(TestJsonChecker, rejects_all_fail_cases) {
	for (const auto& test_case : kFailCases)
		EXPECT_FALSE(parses_as_json_checker_document(test_case.content)) << test_case.name;
}