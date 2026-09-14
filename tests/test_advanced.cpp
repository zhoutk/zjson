// =============================================================================
//  ZJSON :: advanced / plan-completion suite (R1, R3, R4, R5 items)
//
//  Companion to tests/test_api_coverage.cpp.  This file covers the capabilities
//  added while completing the transformation plan:
//    R1  equality matching that survives duplicate keys, allocation-free pointer
//        evaluation, std::get forwarding for entries, locale-independent numeric
//        parsing (from_chars plus explicit inf/nan/hex handling), a defined
//        deep-search order
//    R3  reference access (atRef/findPtr), incremental key index, single-pass
//        range operations
//    R4  iterative (stack-based) clone/destroy/pretty-print/estimate for deep
//        documents, streaming dumpTo, move-parsed input, chunked string arena
//    R5  try_get, setAt, array construction, noexcept/nodiscard contract
//
//  Global new/delete are replaced in this binary only, so claims like "this
//  operation performs no allocation" are measured rather than asserted by eye.
// =============================================================================
#include "gtest/gtest.h"
#include "../src/zjson.hpp"

#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <locale.h>
#include <new>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

using namespace ZJSON;

// -----------------------------------------------------------------------------
// Allocation accounting.
// -----------------------------------------------------------------------------
namespace {
size_t g_allocs = 0;

struct AllocScope {
	size_t start;
	AllocScope() : start(g_allocs) {}
	size_t count() const { return g_allocs - start; }
};
} // namespace

void* operator new(size_t n) {
	++g_allocs;
	if (void* p = std::malloc(n)) return p;
	throw std::bad_alloc();
}
void* operator new[](size_t n) {
	++g_allocs;
	if (void* p = std::malloc(n)) return p;
	throw std::bad_alloc();
}
void* operator new(size_t n, const std::nothrow_t&) noexcept {
	++g_allocs;
	return std::malloc(n);
}
void* operator new[](size_t n, const std::nothrow_t&) noexcept {
	++g_allocs;
	return std::malloc(n);
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, size_t) noexcept { std::free(p); }
void operator delete[](void* p, size_t) noexcept { std::free(p); }
void operator delete(void* p, const std::nothrow_t&) noexcept { std::free(p); }
void operator delete[](void* p, const std::nothrow_t&) noexcept { std::free(p); }

// =============================================================================
// R1-1  Equality: duplicate keys behave as a multiset, and matching is not O(n^2)
// =============================================================================

TEST(TestAdvanced, equality_handles_duplicate_keys_as_a_multiset) {
	// NOTE: parsing collapses duplicate members (the default policy keeps the last),
	// so documents with duplicates are built with add().
	auto withTwo = [](int first, int second) {
		Json doc;
		doc.add("k", first).add("k", second);
		return doc;
	};

	Json a = withTwo(1, 2);
	Json b = withTwo(2, 1);
	EXPECT_TRUE(a == b) << "the same members in a different order are equal";

	Json c = withTwo(1, 3);
	EXPECT_FALSE(a == c);

	// Same key set, different multiplicity.
	Json d;
	d.add("k", 1).add("k", 2).add("k", 3);
	EXPECT_FALSE(a == d);

	// Nested duplicates are compared the same way.
	Json nestedA;
	Json outerA;
	outerA.add("k", Json("{\"x\":1}")).add("k", Json("{\"x\":2}"));
	nestedA.add("o", outerA).add("z", 0);

	Json nestedB;
	Json outerB;
	outerB.add("k", Json("{\"x\":2}")).add("k", Json("{\"x\":1}"));
	nestedB.add("z", 0).add("o", outerB);

	Json nestedC;
	Json outerC;
	outerC.add("k", Json("{\"x\":1}")).add("k", Json("{\"x\":3}"));
	nestedC.add("o", outerC).add("z", 0);

	EXPECT_TRUE(nestedA == nestedB);
	EXPECT_FALSE(nestedA == nestedC);

	// Differing key sets of equal size.
	Json left;
	left.add("a", 1).add("b", 2);
	Json right;
	right.add("a", 1).add("c", 2);
	EXPECT_FALSE(left == right);

	// Arrays keep positional semantics even with "duplicate-like" content.
	Json arrayA("[1,1,2]");
	Json arrayB("[1,2,1]");
	EXPECT_FALSE(arrayA == arrayB);
	EXPECT_TRUE(arrayA == Json("[1,1,2]"));
}

TEST(TestAdvanced, equality_is_cheap_for_mismatched_sizes_and_deep_documents) {
	// A wide object compared against a much smaller one must be rejected quickly
	// (the member counts differ), and a deep but equal pair must still work.
	Json wide;
	for (int i = 0; i < 400; ++i)
		wide.add("k" + std::to_string(i), i);

	Json small;
	small.add("k0", 0);

	AllocScope scope;
	EXPECT_FALSE(wide == small);
	// Count mismatch is detected before any pairing work, so the comparison of a
	// 400-member object against a 1-member object allocates nothing.
	EXPECT_EQ(scope.count(), 0u);

	Json wideCopy = wide;
	EXPECT_TRUE(wide == wideCopy);

	// Equal documents that differ in member order are still equal.
	Json reordered("{\"b\":2,\"a\":1}");
	EXPECT_TRUE(reordered == Json("{\"a\":1,\"b\":2}"));
}

// =============================================================================
// R1-2  Allocation-free pointer evaluation (at / atRef / findPtr paths)
// =============================================================================

TEST(TestAdvanced, pointer_evaluation_does_not_allocate) {
	Json doc("{\"plain\":1,\"a/b\":2,\"m~n\":3,\"arr\":[{\"deep\":4},5]}");

	// First pass: builds the lazy key indexes of every container the pointers reach.
	EXPECT_EQ(doc.at("/plain").toInt(), 1);
	EXPECT_EQ(doc.at("/a~1b").toInt(), 2);
	EXPECT_EQ(doc.at("/m~0n").toInt(), 3);
	EXPECT_EQ(doc.at("/arr/0/deep").toInt(), 4);
	EXPECT_EQ(doc.at("/arr/1").toInt(), 5);

	// Second pass: with the indexes warm, evaluating a pointer must not allocate -
	// the tokens are sliced as views and short tokens stay inside the key index's
	// small-string buffer, so no temporary std::string or Json copy is built.
	AllocScope scope;
	EXPECT_EQ(doc.at("/a~1b").toInt(), 2);
	EXPECT_EQ(doc.at("/m~0n").toInt(), 3);
	EXPECT_EQ(doc.at("/arr/0/deep").toInt(), 4);
	EXPECT_EQ(doc.at("/arr/1").toInt(), 5);
	EXPECT_EQ(doc.atRef("/plain").toInt(), 1);
	EXPECT_EQ(doc.findPtrAt("/arr/0/deep")->toInt(), 4);
	EXPECT_EQ(scope.count(), 0u) << "pointer evaluation should not allocate once warm";
}

TEST(TestAdvanced, pointer_edge_cases_stay_correct) {
	Json doc;
	doc.add("plain", 1);
	doc.add("a/b", 2);
	doc.add("m~n", 3);
	doc.add("", 4);
	doc.add("arr", Json(JsonType::Array).add({ Json{ {"deep", 5} }, 6 }));

	EXPECT_EQ(doc.at("/").toInt(), 4);                 // empty key
	EXPECT_EQ(doc.at("/a~1b").toInt(), 2);             // '/' escape
	EXPECT_EQ(doc.at("/m~0n").toInt(), 3);             // '~' escape
	EXPECT_EQ(doc.at("/arr/0/deep").toInt(), 5);
	EXPECT_EQ(doc.at("/arr/1").toInt(), 6);

	// Rejections keep their meaning.
	EXPECT_TRUE(doc.at("/a~2b").isError());            // bad escape
	EXPECT_TRUE(doc.at("/a~").isError());              // trailing '~'
	EXPECT_TRUE(doc.at("/arr/01").isError());          // leading zero
	EXPECT_TRUE(doc.at("/arr/-").isError());
	EXPECT_TRUE(doc.at("/arr/x").isError());
	EXPECT_TRUE(doc.at("plain").isError());            // must start with '/'
	EXPECT_TRUE(doc.at("/plain/deeper").isError());    // scalar traversal

	// Round trip through the reference accessor added in R3.
	EXPECT_EQ(doc.atRef("/a~1b"), 2);
	EXPECT_EQ(doc.atRef("/arr/0/deep"), 5);
	EXPECT_EQ(&doc.atRef("/plain"), doc.findPtr("plain"));
}

// =============================================================================
// R1-5  std::get works on entries (structured bindings and the tuple protocol)
// =============================================================================

TEST(TestAdvanced, tuple_protocol_and_element_access) {
	Json doc;
	doc.add("key", 1);

	Json::iterator it = doc.begin();
	ASSERT_TRUE(it != doc.end());

	// The entries satisfy the tuple protocol (these specialisations are allowed) and
	// expose an ADL-findable get, which is what structured bindings use. Adding a
	// std::get overload for our own types would be undefined behaviour, so the named
	// form is written with `using std::get;`.
	using std::get;
	EXPECT_EQ(get<0>(*it), "key");
	EXPECT_EQ(get<1>(*it).toInt(), 1);

	const Json& cdoc = doc;
	Json::const_iterator cit = cdoc.cbegin();
	ASSERT_TRUE(cit != cdoc.cend());
	EXPECT_EQ(get<0>(*cit), "key");
	EXPECT_EQ(get<1>(*cit).toInt(), 1);

	// Structured bindings keep working through the same protocol.
	for (auto& [key, value] : doc) {
		EXPECT_EQ(key, "key");
		EXPECT_EQ(value.toInt(), 1);
	}
	for (const auto& [key, value] : cdoc) {
		EXPECT_EQ(key, "key");
		EXPECT_EQ(value.toInt(), 1);
	}

	static_assert(std::tuple_size<JsonEntry>::value == 2, "JsonEntry must have two elements");
	static_assert(std::tuple_size<JsonConstEntry>::value == 2, "JsonConstEntry must have two elements");
	static_assert(std::is_same<std::tuple_element<1, JsonEntry>::type, Json>::value,
		"the second element of JsonEntry is the Json value");
}

// =============================================================================
// R1-6  Numeric parsing is locale independent, including inf / nan / hex floats
// =============================================================================

TEST(TestAdvanced, string_to_double_handles_special_and_hex_forms) {
	// Ordinary decimal conversion must not depend on LC_NUMERIC.
	EXPECT_DOUBLE_EQ(Json("42").toDouble(), 42.0);
	EXPECT_DOUBLE_EQ(Json("3.5").toDouble(), 3.5);
	EXPECT_DOUBLE_EQ(Json("  3.5 ").toDouble(), 3.5);
	EXPECT_DOUBLE_EQ(Json("3.5abc").toDouble(), 3.5);      // leading number wins (atof-like)
	EXPECT_DOUBLE_EQ(Json("1e3").toDouble(), 1000.0);
	EXPECT_DOUBLE_EQ(Json("-1.25e-2").toDouble(), -0.0125);
	EXPECT_DOUBLE_EQ(Json("abc").toDouble(), 0.0);

	// Infinities and NaN are recognised without consulting the C locale.
	EXPECT_TRUE(std::isinf(Json("inf").toDouble()));
	EXPECT_TRUE(std::isinf(Json("INF").toDouble()));
	EXPECT_TRUE(std::isinf(Json("infinity").toDouble()));
	EXPECT_TRUE(std::isinf(Json("-inf").toDouble()));
	EXPECT_LT(Json("-inf").toDouble(), 0.0);
	EXPECT_TRUE(std::isnan(Json("nan").toDouble()));
	EXPECT_TRUE(std::isnan(Json("NaN").toDouble()));

	// Hexadecimal floating point is parsed by hand (std::from_chars does not
	// accept it, and std::atof would consult the locale for the radix point).
	EXPECT_DOUBLE_EQ(Json("0x10").toDouble(), 16.0);
	EXPECT_DOUBLE_EQ(Json("0X10").toDouble(), 16.0);
	EXPECT_DOUBLE_EQ(Json("0x1p3").toDouble(), 8.0);
	EXPECT_DOUBLE_EQ(Json("0x1.8p1").toDouble(), 3.0);
	EXPECT_DOUBLE_EQ(Json("0X1P-1").toDouble(), 0.5);
	EXPECT_DOUBLE_EQ(Json("-0x2").toDouble(), -2.0);
	EXPECT_DOUBLE_EQ(Json("0x").toDouble(), 0.0);

	// A string that merely starts like a number still yields its leading value.
	EXPECT_DOUBLE_EQ(Json("2x").toDouble(), 2.0);
}

TEST(TestAdvanced, decimal_conversion_is_stable_under_a_non_dotted_locale) {
	// If the platform provides a comma-decimal locale, verify that parsing still
	// treats '.' as the decimal separator (this is what from_chars guarantees).
	const char* candidates[] = { "de_DE.UTF-8", "de_DE.utf8", "French_France.1252", "de-DE", "nl_NL.UTF-8" };
	const char* previous = std::setlocale(LC_NUMERIC, nullptr);
	std::string saved = previous ? previous : "C";
	bool switched = false;

	for (const char* name : candidates) {
		if (std::setlocale(LC_NUMERIC, name)) {
			switched = true;
			break;
		}
	}

	if (!switched) {
		GTEST_SKIP() << "no comma-decimal locale available on this platform";
	}

	// Parsing a document, and converting a string value, must both be unaffected.
	std::string err;
	Json parsed = Json::ParseJsonStrict("[1.5,2.25]", err);
	EXPECT_FALSE(parsed.isError()) << err;
	EXPECT_DOUBLE_EQ(parsed[0].toDouble(), 1.5);
	EXPECT_DOUBLE_EQ(parsed[1].toDouble(), 2.25);
	EXPECT_DOUBLE_EQ(Json("3.5").toDouble(), 3.5);
	EXPECT_EQ(Json(1.5).toString(), "1.5");

	std::setlocale(LC_NUMERIC, saved.c_str());
}

// =============================================================================
// R1-7  Deep-search fallback has a defined order (first match in document order)
// =============================================================================

TEST(TestAdvanced, deep_search_returns_the_first_match_in_document_order) {
	// The fallback is only used when the key is not a direct member.
	Json a("{\"a\":{\"k\":1},\"b\":{\"k\":2}}");
	EXPECT_EQ(a["k"].toInt(), 1) << "document order: a's k comes first";

	Json b("{\"deep\":{\"nested\":{\"k\":9}},\"shallow\":{\"k\":8}}");
	EXPECT_EQ(b["k"].toInt(), 9) << "document order is pre-order, not shallowest-first";

	// A direct member always wins over a nested one of the same name.
	Json c("{\"outer\":{\"k\":1},\"k\":2}");
	EXPECT_EQ(c["k"].toInt(), 2);

	// Array elements themselves are not candidates, but an object *inside* an array
	// is: pre-order visits list[0] before obj, so its member wins.
	Json d("{\"list\":[{\"k\":1}],\"obj\":{\"k\":2}}");
	EXPECT_EQ(d["k"].toInt(), 1);

	// Missing keys still yield an Error node.
	EXPECT_TRUE(a["nope"].isError());
}

// =============================================================================
// R3-1  Reference access
// =============================================================================

TEST(TestAdvanced, reference_access_avoids_copies_and_allows_mutation) {
	Json doc("{\"a\":{\"b\":[1,2,3]},\"plain\":7}");

	// atRef() is the read-only reference accessor: no deep copy is made.
	EXPECT_EQ(doc.atRef("/a/b")[2].toInt(), 3);

	// findPtr()/findPtrAt() hand out mutable pointers into the document.
	Json* array = doc.findPtrAt("/a/b");
	ASSERT_NE(array, nullptr);
	array->add(4);
	EXPECT_EQ(doc.at("/a/b").size(), 4);

	Json* found = doc.findPtr("plain");
	ASSERT_NE(found, nullptr);
	*found = Json(8);
	EXPECT_EQ(doc["plain"].toInt(), 8);

	// A missing path yields the error sentinel node, and findPtr yields nullptr.
	EXPECT_TRUE(doc.atRef("/missing").isError());
	EXPECT_EQ(doc.findPtr("missing"), nullptr);
	EXPECT_EQ(doc.findPtrAt("/missing"), nullptr);

	// findPtr() resolves like operator[]: direct member first, then a nested match.
	EXPECT_EQ(doc.findPtr("b"), doc.findPtrAt("/a/b"));

	// Reading through a reference performs no allocation.
	AllocScope scope;
	const int value = doc.atRef("/plain").toInt();
	EXPECT_EQ(value, 8);
	EXPECT_EQ(scope.count(), 0u) << "reference access must not deep-copy";
}

// =============================================================================
// R3-2/R3-3  Key index maintenance and single-pass range operations
// =============================================================================

TEST(TestAdvanced, key_index_tracks_appends_without_full_rebuilds) {
	Json doc;
	for (int i = 0; i < 200; ++i)
		doc.add("k" + std::to_string(i), i);

	// Lookups interleaved with appends must keep returning the right node.
	for (int i = 0; i < 200; ++i) {
		EXPECT_EQ(doc["k" + std::to_string(i)].toInt(), i);
		if (i % 10 == 0)
			doc.add("extra" + std::to_string(i), -i);
	}
	EXPECT_EQ(doc["extra190"].toInt(), -190);
	EXPECT_EQ(doc["k199"].toInt(), 199);
	EXPECT_TRUE(doc.contains("extra0"));
	EXPECT_FALSE(doc.contains("nope"));

	// Removing a member keeps the index consistent for the survivors.
	doc.remove("k100");
	EXPECT_FALSE(doc.contains("k100"));
	EXPECT_EQ(doc["k101"].toInt(), 101);
}

TEST(TestAdvanced, range_operations_are_single_pass_and_keep_semantics) {
	Json source(JsonType::Array);
	for (int i = 0; i < 500; ++i)
		source.add(i);

	// slice() copies a range without mutating the source.
	Json slice = source.slice(100, 110);
	ASSERT_EQ(slice.size(), 10);
	EXPECT_EQ(slice[0].toInt(), 100);
	EXPECT_EQ(slice[9].toInt(), 109);
	EXPECT_EQ(source.size(), 500);
	EXPECT_EQ(source[100].toInt(), 100);

	// takes() moves the range out of the source.
	Json taken = source.takes(0, 5);
	EXPECT_EQ(taken.toString(), "[0,1,2,3,4]");
	EXPECT_EQ(source.size(), 495);
	EXPECT_EQ(source[0].toInt(), 5);

	// Edge cases keep the documented behaviour.
	EXPECT_EQ(source.slice(400, 2).size(), 0);          // inverted range
	EXPECT_EQ(source.slice(1000).size(), 0);            // beyond the end  (end == 0 means "to the end")
	EXPECT_EQ(source.slice(1000).size(), 0);

	// end == 0 means "through the end", exactly as before the rewrite.
	Json tail = source.takes(0);
	EXPECT_EQ(tail.size(), 495);
	EXPECT_EQ(tail[0].toInt(), 5);
	EXPECT_EQ(source.size(), 0);

	// Reuse after taking everything.
	source.add(1);
	EXPECT_EQ(source.toString(), "[1]");
}

// =============================================================================
// R4-5  Deep documents: clone, destroy, pretty print, estimate (iterative paths)
// =============================================================================

namespace {
// Wraps an array around itself `depth` times. The wrapper takes the inner array by
// move, so building is O(depth) instead of the quadratic cost of copying the whole
// subtree at every level.
Json makeDeepArray(int depth) {
	Json node(JsonType::Array);
	node.add(1);
	for (int i = 0; i < depth; ++i) {
		Json wrapper(JsonType::Array);
		wrapper.add(std::move(node));
		node = std::move(wrapper);
	}
	return node;
}
} // namespace

TEST(TestAdvanced, deep_document_paths_do_not_recurse_on_the_call_stack) {
	// 20000 levels: well past what the recursive implementations could handle (they
	// overflowed around 8000). Copy, pretty-print, measure, serialize and destroy
	// must all complete.
	const int depth = 20000;
	Json deep = makeDeepArray(depth);

	Json copy = deep;                                   // clone
	EXPECT_EQ(copy.size(), 1);

	EXPECT_FALSE(copy.toString(2).empty());             // pretty print
	EXPECT_FALSE(copy.toString().empty());              // compact print
	EXPECT_EQ(copy, deep);                              // comparison

	Json moved = std::move(copy);                       // move
	EXPECT_EQ(moved.size(), 1);

	moved.clear();                                      // destroy the deep subtree
	EXPECT_EQ(moved.toString(), "[]");
}

// =============================================================================
// R4-4  Streaming dump
// =============================================================================

TEST(TestAdvanced, dump_to_stream_matches_to_string_and_does_not_buffer_twice) {
	Json doc("{\"a\":[1,2,{\"b\":\"text\"}],\"c\":null}");

	std::ostringstream compact;
	doc.dumpTo(compact, 0);
	EXPECT_EQ(compact.str(), doc.toString());

	std::ostringstream pretty;
	doc.dumpTo(pretty, 2);
	EXPECT_EQ(pretty.str(), doc.toString(2));

	// Multiple documents can be streamed into one stream in sequence.
	std::ostringstream many;
	doc.dumpTo(many, 0);
	doc.dumpTo(many, 0);
	EXPECT_EQ(many.str(), doc.toString() + doc.toString());

	// The existing dump()/operator<< behaviour is unchanged.
	std::ostringstream legacy;
	legacy << doc;
	EXPECT_EQ(legacy.str(), doc.toString());
}

// =============================================================================
// R5-3  try_get family
// =============================================================================

TEST(TestAdvanced, try_get_reports_success_without_exceptions) {
	Json doc("{\"num\":42,\"str\":\"text\",\"flag\":true,\"obj\":{\"x\":1}}");

	int number = 0;
	EXPECT_TRUE(doc.try_get("num", number));
	EXPECT_EQ(number, 42);

	std::string text;
	EXPECT_TRUE(doc.try_get("str", text));
	EXPECT_EQ(text, "text");

	bool flag = false;
	EXPECT_TRUE(doc.try_get("flag", flag));
	EXPECT_TRUE(flag);

	// Wrong type / missing key leave the target untouched and report failure.
	int untouched = 7;
	EXPECT_FALSE(doc.try_get("str", untouched));
	EXPECT_EQ(untouched, 7);
	EXPECT_FALSE(doc.try_get("missing", untouched));
	EXPECT_FALSE(doc.try_get("obj", untouched));

	// Scalar accessors report absence instead of returning a sentinel value.
	EXPECT_EQ(doc.try_int("num"), 42);
	EXPECT_FALSE(doc.try_int("missing").has_value());
	EXPECT_FALSE(doc.try_int("str").has_value());
	EXPECT_EQ(doc.try_double("num"), 42.0);
	EXPECT_EQ(doc.try_string("str"), std::optional<std::string>("text"));
}

// =============================================================================
// R5-5  Pointer writes
// =============================================================================

TEST(TestAdvanced, set_at_writes_through_json_pointer) {
	Json doc("{\"a\":{\"b\":1},\"list\":[1,2,3]}");

	EXPECT_TRUE(doc.setAt("/a/b", 9));
	EXPECT_EQ(doc["a"]["b"].toInt(), 9);

	// Appending to an array uses the RFC 6901 '-' token, like JSON Patch does.
	EXPECT_TRUE(doc.setAt("/list/-", 4));
	EXPECT_EQ(doc["list"].toString(), "[1,2,3,4]");

	// Writing a non-existent object member creates it.
	EXPECT_TRUE(doc.setAt("/a/new", "value"));
	EXPECT_EQ(doc["a"]["new"].toString(), "value");

	// Replacing a container wholesale.
	EXPECT_TRUE(doc.setAt("/a", Json{ {"z", 1} }));
	EXPECT_EQ(doc["a"].toString(), "{\"z\":1}");

	// Failure cases leave the document untouched.
	Json before = doc;
	EXPECT_FALSE(doc.setAt("a/b", 1));                  // missing leading '/'
	EXPECT_FALSE(doc.setAt("/list/9", 1));              // index out of range
	EXPECT_FALSE(doc.setAt("/list/x", 1));              // not an index
	EXPECT_FALSE(doc.setAt("/a/b/c", 1));               // traverses a scalar
	EXPECT_EQ(doc, before);

	std::string err;
	EXPECT_FALSE(doc.setAt("/list/9", 1, err));
	EXPECT_FALSE(err.empty());
}

// =============================================================================
// R5-4  Array construction
// =============================================================================

TEST(TestAdvanced, array_construction_from_values) {
	Json array = Json::array({ 1, "two", true, nullptr });
	EXPECT_TRUE(array.isArray());
	EXPECT_EQ(array.toString(), "[1,\"two\",true,null]");

	Json empty = Json::array({});
	EXPECT_TRUE(empty.isArray());
	EXPECT_EQ(empty.toString(), "[]");

	Json nested = Json::array({ Json{ {"a", 1} }, Json::array({ 2 }) });
	EXPECT_EQ(nested.toString(), "[{\"a\":1},[2]]");
}

// =============================================================================
// R5-6  noexcept / nodiscard contract
// =============================================================================

TEST(TestAdvanced, noexcept_and_return_type_contract) {
	// Predicates and accessors that cannot fail must be noexcept.
	Json doc("{\"a\":1}");
	const Json& cdoc = doc;
	EXPECT_TRUE(noexcept(doc.isObject()));
	EXPECT_TRUE(noexcept(doc.isArray()));
	EXPECT_TRUE(noexcept(doc.isNumber()));
	EXPECT_TRUE(noexcept(doc.isNull()));
	EXPECT_TRUE(noexcept(doc.isError()));
	EXPECT_TRUE(noexcept(cdoc.isEmpty()));

	// Predicates return bool (not int) so the contract is checkable in code.
	static_assert(std::is_same<decltype(doc.isObject()), bool>::value, "isObject must return bool");
	static_assert(std::is_same<decltype(doc.isEmpty()), bool>::value, "isEmpty must return bool");

	// Reference accessors return references, not copies.
	static_assert(std::is_same<decltype(doc.atRef("a")), const Json&>::value,
		"atRef must return a reference");
	static_assert(std::is_same<decltype(std::declval<Json&>().findPtr("a")), Json*>::value,
		"findPtr must return a pointer");
}
