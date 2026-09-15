// =============================================================================
//  ZJSON :: three-state Number suite (R5-1)
//
//  The numeric model used to be "double only", so any integer literal beyond
//  2^53 was silently rounded on the way in and stayed rounded on the way out
//  (`9007199254740993` -> `9007199254740992`).  R5-1 adds a kind tag to Number
//  nodes so an integer literal without a fraction or exponent is stored
//  exactly as int64 / uint64 and written back verbatim, while doubles keep the
//  old behaviour (including the `1` / `-0` spellings and the 1e999 -> null
//  degradation).
//
//  What this file locks down:
//    - exact round trip for the whole 64-bit integer range
//    - state selection: when a literal becomes int64, uint64 or double
//    - the fallback to double only when neither integer state can hold it
//    - numeric equality across states (Json(1) == Json(1.0) must keep working,
//      but a value that is *not* representable as a double must never compare
//      equal to the double it would have been rounded to)
//    - the "does not grow the node" contract, enforced by a layout mirror
//    - preservation of the state through copy/move, containers and JSON Patch
// =============================================================================
#include "gtest/gtest.h"
#include "../src/zjson.hpp"

#include <cstdint>
#include <limits>
#include <string>
#include <vector>

using namespace ZJSON;

namespace {

Json parseStrict(const std::string& text) {
	std::string err;
	ParseOptions options;
	options.allowComments = false;
	Json parsed = Json::ParseJson(text, err, options);
	EXPECT_FALSE(parsed.isError()) << text << " -> " << err;
	return parsed;
}

} // namespace

// -----------------------------------------------------------------------------
// Layout contract: the kind tag must live in what used to be alignment padding
// after `type`, so the three-state model costs no memory.
//
// `JsonLayoutMirror` repeats Json's member list with the tag declared as the
// one-byte field it is.  Json and the mirror are laid out by the same rules, so
// their sizes must agree; if the tag is ever promoted to its own word (or
// another member is added to Json without a matching one here), Json becomes
// larger than the mirror and this static_assert fails - the node, and with it
// the slab block size of the node pool, has grown.
// -----------------------------------------------------------------------------
struct JsonLayoutMirror {
	void* brother;
	void* child;
	void* lastChild;
	void* keymap;
	int type;
	ZJSON::detail::NumberKind numberKind;
	ZJSON::detail::StoredString valueString;
	ZJSON::detail::NumberData valueNumber;
	ZJSON::detail::StoredString name;
};

static_assert(sizeof(ZJSON::detail::NumberData) == sizeof(double),
	"the three-state payload must reuse the 8-byte double slot");
static_assert(sizeof(ZJSON::Json) == sizeof(JsonLayoutMirror),
	"the NumberKind tag must fit in the padding after `type`, not grow the node");

TEST(TestNumber, node_size_is_unchanged_by_the_third_state) {
	// Same fact as the static_assert above, restated at run time so the failure is
	// visible in the test report and not only in the build log.
	EXPECT_EQ(sizeof(ZJSON::Json), sizeof(JsonLayoutMirror));
	EXPECT_EQ(sizeof(ZJSON::detail::NumberData), sizeof(double));
}

// -----------------------------------------------------------------------------
// Exactness
// -----------------------------------------------------------------------------

TEST(TestNumber, integers_round_trip_exactly) {
	const std::vector<std::string> exact = {
		"0", "1", "-1", "42", "-42",
		"9007199254740992",        // 2^53      - the last exact double integer
		"9007199254740993",        // 2^53 + 1  - the value that used to be lost
		"1234567890123456789",
		"9223372036854775807",     // INT64_MAX
		"-9223372036854775808",    // INT64_MIN
		"9223372036854775808",     // INT64_MAX + 1 - only uint64 can hold it
		"18446744073709551615",    // UINT64_MAX
	};

	for (const std::string& text : exact) {
		Json parsed = parseStrict(text);
		ASSERT_TRUE(parsed.isNumber()) << text;
		ASSERT_TRUE(parsed.isIntegral()) << text;
		EXPECT_EQ(parsed.toString(), text) << text;

		// And a second generation is stable too.
		EXPECT_EQ(parseStrict(parsed.toString()).toString(), text) << text;
	}
}

TEST(TestNumber, values_beyond_two_to_the_53_are_not_rounded) {
	Json parsed = parseStrict("9007199254740993");
	EXPECT_EQ(parsed.toInt64(), 9007199254740993LL);
	EXPECT_NE(parsed.toInt64(), 9007199254740992LL);

	// toDouble() still exposes the double view of the value, which is where the
	// historical rounding lives; the documented limit is unchanged.
	EXPECT_DOUBLE_EQ(parsed.toDouble(), 9007199254740992.0);

	EXPECT_EQ(parseStrict("9223372036854775807").toInt64(), std::numeric_limits<int64_t>::max());
	EXPECT_EQ(parseStrict("-9223372036854775808").toInt64(), std::numeric_limits<int64_t>::min());
	EXPECT_EQ(parseStrict("18446744073709551615").toUint64(), std::numeric_limits<uint64_t>::max());
	EXPECT_EQ(parseStrict("9223372036854775808").toUint64(), 9223372036854775808ULL);
}

TEST(TestNumber, state_selection_follows_the_literal_form) {
	// Integer literals - no '.', no 'e'/'E' - land in an integer state.
	EXPECT_TRUE(parseStrict("0").isIntegral());
	EXPECT_TRUE(parseStrict("42").isIntegral());
	EXPECT_TRUE(parseStrict("-42").isIntegral());
	EXPECT_TRUE(parseStrict("18446744073709551615").isIntegral());

	// Anything with a fraction or an exponent stays in the double state, even when
	// the value happens to be whole.
	EXPECT_FALSE(parseStrict("42.0").isIntegral());
	EXPECT_FALSE(parseStrict("42e0").isIntegral());
	EXPECT_FALSE(parseStrict("42E0").isIntegral());
	EXPECT_FALSE(parseStrict("4.2").isIntegral());
	EXPECT_FALSE(parseStrict("1e999").isIntegral());

	// Non-numbers are never integral.
	EXPECT_FALSE(parseStrict("\"42\"").isIntegral());
	EXPECT_FALSE(Json(true).isIntegral());
	EXPECT_FALSE(Json(nullptr).isIntegral());
	EXPECT_FALSE(Json(JsonType::Object).isIntegral());
	{
		std::string err;
		EXPECT_FALSE(Json::ParseJson("{bad", err).isIntegral());
	}

	// C++ scalars: integral types keep their exactness, floating point does not.
	EXPECT_TRUE(Json(1).isIntegral());
	EXPECT_TRUE(Json(-1).isIntegral());
	EXPECT_TRUE(Json(1u).isIntegral());
	EXPECT_TRUE(Json(1LL).isIntegral());
	EXPECT_TRUE(Json(1ULL).isIntegral());
	EXPECT_TRUE(Json('6').isIntegral());
	EXPECT_TRUE(Json(std::numeric_limits<uint64_t>::max()).isIntegral());
	EXPECT_FALSE(Json(1.0f).isIntegral());
	EXPECT_FALSE(Json(1.0).isIntegral());
}

TEST(TestNumber, only_literals_that_fit_no_integer_state_become_double) {
	// 2^64 is exactly representable as a double, so the fallback still keeps the
	// value - it just leaves the integer model.
	Json past = parseStrict("18446744073709551616");
	EXPECT_TRUE(past.isNumber());
	EXPECT_FALSE(past.isIntegral());
	EXPECT_EQ(past.toString(), "18446744073709551616");
	EXPECT_DOUBLE_EQ(past.toDouble(), 18446744073709551616.0);

	// Far outside the double range: still accepted, still degrades to null when
	// serialized (unchanged historical behaviour).
	EXPECT_TRUE(parseStrict("1e999").isNumber());
	EXPECT_EQ(parseStrict("1e999").toString(), "null");

	// A 40-digit literal has neither an integer state nor an exact double form;
	// whatever it parses to must at least round-trip as text.
	Json huge = parseStrict("9999999999999999999999999999999999999999");
	EXPECT_TRUE(huge.isNumber());
	EXPECT_FALSE(huge.isIntegral());
	EXPECT_EQ(parseStrict(huge.toString()).toString(), huge.toString());
}

TEST(TestNumber, negative_zero_keeps_its_sign) {
	// "-0" is spelled like an integer but no integer state can carry the sign of
	// zero, so it stays a double and still serializes as -0.
	EXPECT_EQ(parseStrict("-0").toString(), "-0");
	EXPECT_FALSE(parseStrict("-0").isIntegral());
	EXPECT_EQ(parseStrict("-0.0").toString(), "-0");
	EXPECT_EQ(parseStrict("[-0]").toString(), "[-0]");

	// ... while plain zero is an integer and prints without the sign.
	EXPECT_EQ(parseStrict("0").toString(), "0");
	EXPECT_TRUE(parseStrict("0").isIntegral());
	EXPECT_EQ(Json(0).toString(), "0");
	EXPECT_EQ(Json(-0.0).toString(), "-0");
}

// -----------------------------------------------------------------------------
// Equality
// -----------------------------------------------------------------------------

TEST(TestNumber, equality_is_numeric_across_states) {
	EXPECT_TRUE(Json(1) == Json(1.0));
	EXPECT_TRUE(Json(1) == Json(1u));
	EXPECT_TRUE(Json(1) == Json(1LL));
	EXPECT_TRUE(Json(1) == Json(1ULL));
	EXPECT_TRUE(Json(0) == Json(0.0));
	EXPECT_TRUE(Json(0) == Json(-0.0));
	EXPECT_TRUE(Json(2.0) == Json(2));
	EXPECT_TRUE(Json(18446744073709551615ULL) == Json(18446744073709551615ULL));
	EXPECT_TRUE(Json(static_cast<int64_t>(-1)) != Json(std::numeric_limits<uint64_t>::max()));

	// The crux of R5-1: a value that is not representable as a double must not
	// compare equal to the double it would have been rounded to.
	EXPECT_TRUE(parseStrict("9007199254740993") == parseStrict("9007199254740993"));
	EXPECT_TRUE(parseStrict("9007199254740993") != parseStrict("9007199254740992"));
	EXPECT_TRUE(parseStrict("9007199254740993") != parseStrict("9007199254740992.0"));

	// Cross-state equality still holds for values both states can express.
	EXPECT_TRUE(parseStrict("1") == parseStrict("1.0"));
	EXPECT_TRUE(parseStrict("-7") == parseStrict("-7.0"));
	EXPECT_TRUE(parseStrict("9007199254740992") == parseStrict("9007199254740992.0"));

	// A double outside the 64-bit integer range equals no integer.
	EXPECT_TRUE(Json(std::numeric_limits<int64_t>::max()) != Json(9223372036854775808.0));
	EXPECT_TRUE(Json(parseStrict("1.5")) != Json(1));
}

// -----------------------------------------------------------------------------
// Serialization of C++ scalars
// -----------------------------------------------------------------------------

TEST(TestNumber, scalar_serialization_is_unchanged) {
	EXPECT_EQ(Json(7).toString(), "7");
	EXPECT_EQ(Json(-7).toString(), "-7");
	EXPECT_EQ(Json(0).toString(), "0");
	EXPECT_EQ(Json(7u).toString(), "7");
	EXPECT_EQ(Json(short(-7)).toString(), "-7");
	EXPECT_EQ(Json('6').toString(), "54");
	EXPECT_EQ(Json(123456789L).toInt(), 123456789);
	EXPECT_EQ(Json(1234567890123LL).toString(), "1234567890123");
	EXPECT_EQ(Json(4294967295u).toString(), "4294967295");
	EXPECT_EQ(Json(std::numeric_limits<int64_t>::min()).toString(), "-9223372036854775808");
	EXPECT_EQ(Json(std::numeric_limits<uint64_t>::max()).toString(), "18446744073709551615");

	// Doubles keep their fast paths: whole values print without a fraction, and
	// negative zero keeps its sign.
	EXPECT_EQ(Json(1.0).toString(), "1");
	EXPECT_EQ(Json(-1.5).toString(), "-1.5");
	EXPECT_EQ(Json(0.1).toString(), "0.1");
	EXPECT_EQ(Json(1e10).toString(), "10000000000");
	EXPECT_EQ(Json(1e-7).toString(), "1e-07");
	EXPECT_EQ(Json(1.0e300).toString(), "1e+300");
	EXPECT_EQ(Json(std::nan("")).toString(), "null");
	EXPECT_EQ(Json(HUGE_VAL).toString(), "null");
}

TEST(TestNumber, conversions_use_the_stored_state) {
	EXPECT_EQ(parseStrict("42").toInt(), 42);
	EXPECT_EQ(parseStrict("-42").toInt(), -42);
	EXPECT_EQ(parseStrict("3.9").toInt(), 3);
	EXPECT_EQ(parseStrict("-3.9").toInt(), -3);
	EXPECT_DOUBLE_EQ(parseStrict("42").toDouble(), 42.0);
	EXPECT_DOUBLE_EQ(parseStrict("18446744073709551615").toDouble(), 18446744073709551616.0);
	EXPECT_DOUBLE_EQ(parseStrict("2.9").toDouble(), 2.9);

	// toInt64/toUint64 follow C++ cast semantics for non-integer states.
	EXPECT_EQ(Json(2.9).toInt64(), 2);
	EXPECT_EQ(Json(1.5).toUint64(), 1ULL);
	EXPECT_EQ(Json(true).toInt64(), 1);
	EXPECT_EQ(Json("17").toInt64(), 17);          // string values are parsed, as toInt does

	// try_get reuses the same conversion instead of narrowing through double.
	Json doc;
	doc.add("big", 9007199254740993LL);
	int64_t asInt64 = 0;
	EXPECT_TRUE(doc.try_get("big", asInt64));
	EXPECT_EQ(asInt64, 9007199254740993LL);
	double asDouble = 0;
	EXPECT_TRUE(doc.try_get("big", asDouble));
	EXPECT_DOUBLE_EQ(asDouble, 9007199254740992.0);
}

// -----------------------------------------------------------------------------
// The state survives every path that carries a value around
// -----------------------------------------------------------------------------

TEST(TestNumber, copy_move_and_containers_preserve_the_integer_state) {
	Json doc;
	doc.add("id", std::numeric_limits<int64_t>::max());
	doc.add("count", std::numeric_limits<uint64_t>::max());
	doc.add("small", 7);
	EXPECT_EQ(doc.toString(),
		"{\"id\":9223372036854775807,\"count\":18446744073709551615,\"small\":7}");

	Json copied = doc;                                       // deep copy
	EXPECT_EQ(copied.toString(), doc.toString());
	EXPECT_TRUE(copied["id"].isIntegral());
	EXPECT_EQ(copied["count"].toUint64(), std::numeric_limits<uint64_t>::max());

	Json assigned;
	assigned = doc;                                          // copy assignment
	EXPECT_EQ(assigned.toString(), doc.toString());

	Json moved = std::move(copied);                          // move construction
	EXPECT_EQ(moved["id"].toInt64(), std::numeric_limits<int64_t>::max());
	EXPECT_EQ(moved["count"].toUint64(), std::numeric_limits<uint64_t>::max());

	Json moveAssigned;
	moveAssigned = std::move(moved);                          // move assignment
	EXPECT_EQ(moveAssigned["small"].toInt64(), 7);

	// A container re-serializes byte for byte, and so does a parse of that text.
	Json reparsed = parseStrict(doc.toString());
	EXPECT_EQ(reparsed.toString(), doc.toString());
	EXPECT_EQ(reparsed["count"].toUint64(), std::numeric_limits<uint64_t>::max());

	// extend()/concat() re-add each member by value - the state must survive there
	// too, so the number is not silently narrowed to a double.
	Json extended = Json(JsonType::Object).extend(doc);
	EXPECT_EQ(extended.toString(), doc.toString());
	EXPECT_TRUE(extended["count"].isIntegral());
}

TEST(TestNumber, json_patch_and_merge_patch_carry_the_exact_value) {
	const char* operations =
		"[{\"op\":\"add\",\"path\":\"/big\",\"value\":9007199254740993},"
		" {\"op\":\"add\",\"path\":\"/huge\",\"value\":18446744073709551615},"
		" {\"op\":\"test\",\"path\":\"/big\",\"value\":9007199254740993}]";

	std::string err;
	Json ops = Json::ParseJson(operations, err);
	ASSERT_FALSE(ops.isError()) << err;

	Json result = Json(JsonType::Object).applyPatch(ops, err);
	ASSERT_FALSE(result.isError()) << err;
	EXPECT_EQ(result["big"].toString(), "9007199254740993");
	EXPECT_EQ(result["big"].toInt64(), 9007199254740993LL);
	EXPECT_EQ(result["huge"].toUint64(), std::numeric_limits<uint64_t>::max());

	// setAt() goes through the same patch engine.
	Json target;
	ASSERT_TRUE(target.setAt("/n", Json(9007199254740993LL), err)) << err;
	EXPECT_EQ(target.toString(), "{\"n\":9007199254740993}");

	// A merge patch replaces the value and must not round it either.
	target.mergePatch(Json(9007199254740993LL));
	EXPECT_EQ(target.toString(), "9007199254740993");
}

TEST(TestNumber, large_arrays_of_wide_integers_round_trip) {
	Json array(JsonType::Array);
	for (int i = 0; i < 1000; ++i)
		array.add(9007199254740993LL + i);

	const std::string text = array.toString();
	Json reparsed = parseStrict(text);
	ASSERT_EQ(reparsed.size(), 1000);
	for (int i = 0; i < 1000; ++i)
		EXPECT_EQ(reparsed[i].toInt64(), 9007199254740993LL + i);

	EXPECT_EQ(reparsed.toString(), text);
	EXPECT_TRUE(reparsed[0].isIntegral());
}

// -----------------------------------------------------------------------------
// Review N5: the "not found" sentinel that atRef() returns.
// -----------------------------------------------------------------------------

TEST(TestNumber, error_sentinel_is_one_stable_node_per_thread) {
	Json doc;
	doc.add("a", 1);

	// The sentinel is a single node shared by every failed lookup on this thread, so
	// its address is stable and it is const - callers can compare against it but
	// cannot write through it.
	const Json& first = doc.atRef("/missing");
	const Json& second = doc.atRef("/missing");
	EXPECT_EQ(&first, &second);
	EXPECT_TRUE(first.isError());
	EXPECT_EQ(first.toString(), "");
	EXPECT_FALSE(first.isNumber());

	// A lookup that succeeds never hands out the sentinel.
	const Json& found = doc.atRef("/a");
	EXPECT_NE(&found, &first);
	EXPECT_EQ(found.toInt(), 1);

	// And the sentinel is not reachable from any document.
	EXPECT_FALSE(doc.contains("missing"));
	EXPECT_EQ(doc.size(), -1);            // an object is not an array
	EXPECT_EQ(doc.toString(), "{\"a\":1}");
}
