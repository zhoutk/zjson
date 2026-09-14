// =============================================================================
//  ZJSON :: full API coverage suite
//
//  Goal: every public API entry point, every documented overload and every
//  documented edge case is exercised at least once, with concrete instances, so
//  that "ctest is green" is a meaningful statement about the whole surface.
//
//  This file covers, in order:
//    1. constructors and special member functions
//    2. type queries and conversions
//    3. read access (operator[], contains, size, keys, search helpers)
//    4. mutation (add/push/pop/insert/remove/take/extend/concat/clear)
//    5. JSON Pointer, Merge Patch, JSON Patch
//    6. serialization (compact, pretty, escapes, numbers, streams)
//    7. iterators (range-for, structured bindings, tuple protocol, traits)
//    8. equality
//    9. lifecycle / stress behaviour, including an allocation-growth smoke test
//
//  Parser-focused coverage (strict/extension modes, UTF-8 validation, depth,
//  duplicate-key policies, error positions, file loading) lives in
//  tests/test_parse_coverage.cpp.
// =============================================================================
#include "gtest/gtest.h"
#include "../src/zjson.hpp"

#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <new>
#include <sstream>
#include <string>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

using namespace ZJSON;

// -----------------------------------------------------------------------------
// Allocation accounting.  Global new/delete are replaced in this test binary
// only, so a workload can be repeated and its allocation count compared between
// repetitions: steady state means no per-iteration leak (nodes, keys, keymaps,
// arena strings, temporary buffers all get returned).
// -----------------------------------------------------------------------------
namespace {
size_t g_allocCount = 0;
size_t g_freeCount = 0;
size_t g_allocBytes = 0;

struct AllocSnapshot {
	size_t count;
	size_t bytes;
};

AllocSnapshot allocationSnapshot() { return AllocSnapshot{ g_allocCount, g_allocBytes }; }

size_t allocationsBetween(const AllocSnapshot& from) { return g_allocCount - from.count; }
} // namespace

void* operator new(size_t size) {
	++g_allocCount;
	g_allocBytes += size;
	if (void* p = std::malloc(size)) return p;
	throw std::bad_alloc();
}

void* operator new[](size_t size) {
	++g_allocCount;
	g_allocBytes += size;
	if (void* p = std::malloc(size)) return p;
	throw std::bad_alloc();
}

void* operator new(size_t size, const std::nothrow_t&) noexcept {
	++g_allocCount;
	g_allocBytes += size;
	return std::malloc(size);
}

void* operator new[](size_t size, const std::nothrow_t&) noexcept {
	++g_allocCount;
	g_allocBytes += size;
	return std::malloc(size);
}

void operator delete(void* ptr) noexcept { ++g_freeCount; std::free(ptr); }
void operator delete[](void* ptr) noexcept { ++g_freeCount; std::free(ptr); }
void operator delete(void* ptr, size_t) noexcept { ++g_freeCount; std::free(ptr); }
void operator delete[](void* ptr, size_t) noexcept { ++g_freeCount; std::free(ptr); }
void operator delete(void* ptr, const std::nothrow_t&) noexcept { ++g_freeCount; std::free(ptr); }
void operator delete[](void* ptr, const std::nothrow_t&) noexcept { ++g_freeCount; std::free(ptr); }

// =============================================================================
// 1. Constructors and special member functions
// =============================================================================

TEST(TestApiCoverage, default_and_typed_constructors) {
	Json object;
	EXPECT_TRUE(object.isObject());
	EXPECT_EQ(object.getValueType(), "Object");
	EXPECT_EQ(object.toString(), "{}");

	Json array(JsonType::Array);
	EXPECT_TRUE(array.isArray());
	EXPECT_EQ(array.getValueType(), "Array");
	EXPECT_EQ(array.size(), 0);
	EXPECT_TRUE(array.isEmpty());
	EXPECT_EQ(array.toString(), "[]");

	Json explicitObject(JsonType::Object);
	EXPECT_EQ(explicitObject.toString(), "{}");
	EXPECT_TRUE(object == explicitObject);
}

TEST(TestApiCoverage, text_constructor_detection) {
	// Valid JSON containers are parsed ...
	Json fromText("{\"a\":[1,2,3]}");
	EXPECT_TRUE(fromText.isObject());
	EXPECT_EQ(fromText["a"][2].toInt(), 3);

	// ... including when leading whitespace precedes the container.
	Json padded("\n\t  [1, 2]");
	EXPECT_TRUE(padded.isArray());
	EXPECT_EQ(padded.size(), 2);

	// Text that is not a container is stored as a plain string value.
	Json plain("hello world");
	EXPECT_TRUE(plain.isString());
	EXPECT_EQ(plain.toString(), "hello world");

	// Malformed container text falls back to a string value (documented).
	Json malformed("{\"a\":}");
	EXPECT_TRUE(malformed.isString());
	EXPECT_EQ(malformed.toString(), "{\"a\":}");

	Json empty{ std::string() };
	EXPECT_TRUE(empty.isString());
	EXPECT_EQ(empty.toString(), "");
	EXPECT_TRUE(empty.isEmpty());
}

TEST(TestApiCoverage, null_pointer_constructors) {
	Json fromNull(static_cast<const char*>(nullptr));
	EXPECT_TRUE(fromNull.isNull());
	EXPECT_EQ(fromNull.getValueType(), "Null");

	Json fromNullptr(nullptr);
	EXPECT_TRUE(fromNullptr.isNull());
	EXPECT_EQ(fromNullptr.toString(), "null");
}

TEST(TestApiCoverage, numeric_bool_and_character_constructors) {
	EXPECT_EQ(Json(7).toString(), "7");
	EXPECT_EQ(Json(-7).toString(), "-7");
	EXPECT_EQ(Json(0).toString(), "0");
	EXPECT_EQ(Json(123456789L).toInt(), 123456789);
	EXPECT_EQ(Json(1234567890123LL).toString(), "1234567890123");
	EXPECT_EQ(Json(4294967295u).toString(), "4294967295");
	EXPECT_TRUE(Json(0u).isNumber());

	const float smallFloat = 9.012345f;
	Json fromFloat(smallFloat);
	EXPECT_TRUE(fromFloat.isNumber());
	EXPECT_DOUBLE_EQ(fromFloat.toDouble(), static_cast<double>(smallFloat));

	Json fromDouble(1.5);
	EXPECT_EQ(fromDouble.toDouble(), 1.5);
	EXPECT_EQ(fromDouble.toString(), "1.5");

	// '6' is an integral value, therefore a number (54), not a string.
	EXPECT_EQ(Json('6').toInt(), 54);

	// Non-finite doubles: NaN becomes a Null node, while +/-Inf stays a Number
	// node whose serialization is null (RFC 8259 has no representation for it).
	EXPECT_TRUE(Json(std::nan("")).isNull());
	EXPECT_TRUE(Json(HUGE_VAL).isNumber());
	EXPECT_TRUE(Json(-HUGE_VAL).isNumber());
	EXPECT_EQ(Json(HUGE_VAL).toString(), "null");
	EXPECT_EQ(Json(-HUGE_VAL).toString(), "null");

	EXPECT_TRUE(Json(true).isTrue());
	EXPECT_TRUE(Json(false).isFalse());
	EXPECT_EQ(Json(true).toString(), "true");
	EXPECT_EQ(Json(false).toString(), "false");
}

TEST(TestApiCoverage, initializer_list_object_constructor) {
	Json person{
		{"name", "kevin"},
		{"age", 18},
		{"score", 95.5},
		{"member", true},
		{"nickname", nullptr},
		{"languages", Json(JsonType::Array).add({"zh", "en"})}
	};

	EXPECT_EQ(person.toString(),
		"{\"name\":\"kevin\",\"age\":18,\"score\":95.5,\"member\":true,\"nickname\":null,\"languages\":[\"zh\",\"en\"]}");
	EXPECT_EQ(person["name"].toString(), "kevin");
	EXPECT_EQ(person["age"].toInt(), 18);
	EXPECT_TRUE(person["member"].isTrue());
	EXPECT_TRUE(person["nickname"].isNull());
	EXPECT_EQ(person["languages"][1].toString(), "en");

	// Nested initializer lists build nested containers.
	Json nested{ {"outer", Json{ {"inner", 1} }} };
	EXPECT_EQ(nested.toString(), "{\"outer\":{\"inner\":1}}");

	// The empty key is a legal object key.
	Json emptyKey{ {std::string(), 1} };
	EXPECT_EQ(emptyKey.toString(), "{\"\":1}");
}

TEST(TestApiCoverage, copy_and_move_are_value_semantics) {
	Json original;
	original.add("k1", 1);
	original.add("k2", Json(JsonType::Array).add({1, 2, 3}));

	Json copied(original);                 // copy ctor
	EXPECT_EQ(copied, original);
	copied.add("k3", "extra");
	EXPECT_NE(copied, original);           // deep copy: independent storage
	EXPECT_FALSE(original.contains("k3"));

	Json assigned;
	assigned = original;                   // copy assignment
	EXPECT_EQ(assigned, original);
	assigned["k1"];                        // force a keymap build on the target
	EXPECT_EQ(assigned.toString(), original.toString());

	Json moved(std::move(copied));         // move ctor
	EXPECT_EQ(moved["k2"][2].toInt(), 3);
	EXPECT_TRUE(copied.isObject());        // moved-from stays a valid, empty object
	EXPECT_EQ(copied.toString(), "{}");

	Json moveAssigned;
	moveAssigned = std::move(moved);       // move assignment
	EXPECT_EQ(moveAssigned["k2"][0].toInt(), 1);
	EXPECT_EQ(moved.toString(), "{}");

	// Self assignment must be harmless for both forms.
	Json self;
	self.add("v", 1);
	self = self;
	EXPECT_EQ(self.toString(), "{\"v\":1}");
	self = std::move(self);
	EXPECT_EQ(self.toString(), "{\"v\":1}");

	// Reuse of a moved-from object as an ordinary container.
	Json reused = std::move(copied);
	reused.add("again", 1);
	EXPECT_EQ(reused.toString(), "{\"again\":1}");
}

// =============================================================================
// 2. Type queries and conversions
// =============================================================================

TEST(TestApiCoverage, type_predicates_for_every_type) {
	std::string err;
	Json error = Json::ParseJson("{bad", err);
	EXPECT_TRUE(error.isError());
	EXPECT_EQ(error.getValueType(), "Error");
	EXPECT_EQ(error.toString(), "");

	Json booleanFalse(false);
	EXPECT_TRUE(booleanFalse.isFalse());
	EXPECT_FALSE(booleanFalse.isTrue());
	EXPECT_EQ(booleanFalse.getValueType(), "False");

	Json booleanTrue(true);
	EXPECT_TRUE(booleanTrue.isTrue());
	EXPECT_FALSE(booleanTrue.isFalse());
	EXPECT_EQ(booleanTrue.getValueType(), "True");

	Json nullValue(nullptr);
	EXPECT_TRUE(nullValue.isNull());
	EXPECT_EQ(nullValue.getValueType(), "Null");

	Json number(1);
	EXPECT_TRUE(number.isNumber());
	EXPECT_FALSE(number.isString());
	EXPECT_EQ(number.getValueType(), "Number");

	Json text("text");
	EXPECT_TRUE(text.isString());
	EXPECT_EQ(text.getValueType(), "String");

	Json object;
	EXPECT_TRUE(object.isObject());
	EXPECT_EQ(object.getValueType(), "Object");

	Json array(JsonType::Array);
	EXPECT_TRUE(array.isArray());
	EXPECT_EQ(array.getValueType(), "Array");

	// Every predicate is false for the other seven kinds.
	EXPECT_FALSE(number.isObject());
	EXPECT_FALSE(number.isArray());
	EXPECT_FALSE(number.isNull());
	EXPECT_FALSE(number.isError());
	EXPECT_FALSE(number.isTrue());
	EXPECT_FALSE(number.isFalse());
	EXPECT_FALSE(text.isNumber());
}

TEST(TestApiCoverage, size_and_isEmpty_semantics) {
	Json array(JsonType::Array);
	EXPECT_EQ(array.size(), 0);
	EXPECT_TRUE(array.isEmpty());
	array.add(1).add(2).add(3);
	EXPECT_EQ(array.size(), 3);
	EXPECT_FALSE(array.isEmpty());

	// Documented historical behaviour: size() only counts arrays and reports -1
	// for every other kind, and isEmpty() is therefore true for non-arrays.
	Json object;
	object.add("a", 1);
	EXPECT_EQ(object.size(), -1);
	EXPECT_TRUE(object.isEmpty());

	EXPECT_EQ(Json(1).size(), -1);
	EXPECT_TRUE(Json(1).isEmpty());
	EXPECT_EQ(Json("text").size(), -1);
	EXPECT_TRUE(Json(nullptr).isEmpty());
}

TEST(TestApiCoverage, scalar_conversions) {
	// Numbers convert to each scalar type.
	Json number(42.75);
	EXPECT_EQ(number.toInt(), 42);
	EXPECT_EQ(number.toDouble(), 42.75);
	EXPECT_FLOAT_EQ(number.toFloat(), 42.75f);
	EXPECT_FALSE(number.toBool());          // toBool() is type-strict: only True

	// Strings that contain numbers are converted, others yield 0.
	EXPECT_EQ(Json("42").toInt(), 42);
	EXPECT_EQ(Json("42").toDouble(), 42.0);
	EXPECT_EQ(Json("  3.5 ").toDouble(), 3.5);
	EXPECT_EQ(Json("3.5abc").toDouble(), 3.5);   // atof-like: leading number wins
	EXPECT_EQ(Json("abc").toDouble(), 0.0);
	EXPECT_EQ(Json("").toDouble(), 0.0);
	EXPECT_EQ(Json("0").toInt(), 0);

	// Booleans: only True/False convert; strings never do.
	EXPECT_TRUE(Json(true).toBool());
	EXPECT_FALSE(Json(false).toBool());
	EXPECT_FALSE(Json("true").toBool());
	EXPECT_EQ(Json(true).toInt(), 1);
	EXPECT_EQ(Json(false).toInt(), 0);
	EXPECT_EQ(Json(true).toDouble(), 1.0);

	// Containers and null yield 0 / false.
	EXPECT_EQ(Json(nullptr).toInt(), 0);
	EXPECT_EQ(Json(nullptr).toBool(), false);
	EXPECT_EQ(Json(JsonType::Array).add({1, 2}).toDouble(), 0.0);
	EXPECT_EQ(Json(JsonType::Object).toInt(), 0);
}

TEST(TestApiCoverage, to_vector_and_get_all_keys) {
	Json array(JsonType::Array);
	array.add(1).add("two").add(true);
	std::vector<Json> values = array.toVector();
	ASSERT_EQ(values.size(), 3u);
	EXPECT_EQ(values[0].toInt(), 1);
	EXPECT_EQ(values[1].toString(), "two");
	EXPECT_TRUE(values[2].isTrue());

	// toVector() on a non-array yields an empty vector.
	EXPECT_TRUE(Json(1).toVector().empty());
	EXPECT_TRUE(Json(JsonType::Object).toVector().empty());

	Json object;
	object.add("alpha", 1).add("beta", 2);
	Json keys = object.getAllKeys();
	ASSERT_EQ(keys.size(), 2);
	EXPECT_EQ(keys[0].toString(), "alpha");
	EXPECT_EQ(keys[1].toString(), "beta");

	// Non-objects expose no keys.
	EXPECT_EQ(Json(JsonType::Array).add(1).getAllKeys().size(), 0);
	EXPECT_EQ(Json(1).getAllKeys().size(), 0);

	// Empty keys are part of the key list.
	Json parsedEmptyKey("{\"\":1,\"k\":2}");
	ASSERT_TRUE(parsedEmptyKey.isObject());
	Json allKeys = parsedEmptyKey.getAllKeys();
	ASSERT_EQ(allKeys.size(), 2);
	EXPECT_EQ(allKeys[0].toString(), "");
	EXPECT_EQ(allKeys[1].toString(), "k");
}

// =============================================================================
// 3. Read access
// =============================================================================

TEST(TestApiCoverage, object_subscript_returns_a_copy) {
	Json doc("{\"a\":{\"b\":1}}");
	Json value = doc["a"];
	EXPECT_EQ(value["b"].toInt(), 1);

	// The returned node is a copy: mutating it must not change the document.
	value.add("c", 2);
	EXPECT_FALSE(doc["a"].contains("c"));
	EXPECT_EQ(doc.toString(), "{\"a\":{\"b\":1}}");
}

TEST(TestApiCoverage, object_subscript_rules) {
	Json doc("{\"a\":1,\"nested\":{\"deep\":2},\"arr\":[{\"inArray\":3}]}");

	EXPECT_EQ(doc["a"].toInt(), 1);
	EXPECT_EQ(doc["nested"]["deep"].toInt(), 2);
	EXPECT_EQ(doc["deep"].toInt(), 2);            // deep-search fallback
	EXPECT_EQ(doc["arr"][0]["inArray"].toInt(), 3);

	EXPECT_TRUE(doc["missing"].isError());
	EXPECT_TRUE(doc[""].isError());               // no empty key in this document

	// Subscript on non-objects is an Error node.
	EXPECT_TRUE(Json(JsonType::Array).add(1)["a"].isError());
	EXPECT_TRUE(Json(1)["a"].isError());
	EXPECT_TRUE(Json("text")["a"].isError());
	EXPECT_TRUE(Json(nullptr)["a"].isError());

	// Subscript on an empty object is an Error node, not a crash.
	EXPECT_TRUE(Json(JsonType::Object)["a"].isError());
}

TEST(TestApiCoverage, object_subscript_after_mutations_keeps_keymap_consistent) {
	Json doc;
	doc.add("a", 1);
	EXPECT_EQ(doc["a"].toInt(), 1);              // builds the lazy keymap

	doc.add("b", 2);                             // append invalidates the keymap
	EXPECT_EQ(doc["b"].toInt(), 2);
	EXPECT_EQ(doc["a"].toInt(), 1);

	doc.remove("a");                             // removal invalidates it again
	EXPECT_TRUE(doc["a"].isError());
	EXPECT_FALSE(doc.contains("a"));
	EXPECT_EQ(doc["b"].toInt(), 2);

	doc.add("c", 3);
	EXPECT_EQ(doc.toString(), "{\"b\":2,\"c\":3}");
	EXPECT_EQ(doc["c"].toInt(), 3);

	// Replacing a member invalidates the keymap as well.
	Json patch{ {"b", 20} };
	doc.mergePatch(patch);
	EXPECT_EQ(doc["b"].toInt(), 20);
	EXPECT_EQ(doc.toString(), "{\"b\":20,\"c\":3}");
}

TEST(TestApiCoverage, array_subscript_rules) {
	Json array(JsonType::Array);
	array.add(10).add(20).add(30);

	EXPECT_EQ(array[0].toInt(), 10);
	EXPECT_EQ(array[2].toInt(), 30);
	EXPECT_TRUE(array[3].isError());
	EXPECT_TRUE(array[100].isError());
	EXPECT_TRUE(array[-1].isError());

	Json empty(JsonType::Array);
	EXPECT_TRUE(empty[0].isError());
	EXPECT_TRUE(empty[1].isError());
	EXPECT_TRUE(empty[-1].isError());

	// Subscript on a non-array (including index subscript on an object) is an
	// Error node.
	std::string err;
	Json parsedObject = Json::ParseJson("{\"0\":\"zero\"}", err);
	ASSERT_FALSE(parsedObject.isError()) << err;
	EXPECT_EQ(parsedObject["0"].toString(), "zero");
	EXPECT_TRUE(parsedObject["key"].isError());
	Json numericKeyObject{ {"0", "zero"} };
	EXPECT_TRUE(numericKeyObject[0].isError());
	EXPECT_TRUE(Json(JsonType::Array).add(1)[0].isNumber());
	EXPECT_TRUE(Json(JsonType::Object)["a"].isError());
}

TEST(TestApiCoverage, contains_rules) {
	Json doc("{\"a\":1,\"nested\":{\"b\":2}}");
	EXPECT_TRUE(doc.contains("a"));
	EXPECT_FALSE(doc.contains("b"));             // contains() is direct-level
	EXPECT_TRUE(doc.contains("nested"));
	EXPECT_FALSE(doc.contains("missing"));
	EXPECT_FALSE(doc.contains(""));

	Json empty;
	EXPECT_FALSE(empty.contains("a"));

	// contains() is object-only.
	EXPECT_FALSE(Json(JsonType::Array).add(1).contains("a"));
	EXPECT_FALSE(Json(1).contains("a"));

	Json emptyKey("{\"\":1}");
	EXPECT_TRUE(emptyKey.contains(""));
	EXPECT_EQ(emptyKey[""].toInt(), 1);
}

TEST(TestApiCoverage, search_helpers_first_last_index_of_slice) {
	Json array(JsonType::Array);
	array.add({ 0, 1, 2, 3, 4 });

	EXPECT_EQ(array.first().toInt(), 0);
	EXPECT_EQ(array.last().toInt(), 4);
	EXPECT_EQ(array.indexOf("3"), 3);
	EXPECT_EQ(array.indexOf("0"), 0);
	EXPECT_EQ(array.indexOf("9"), -1);
	EXPECT_EQ(array.indexOf(""), -1);

	// indexOf() compares the textual form: numbers, strings, literals and
	// containers all match their serialized representation.
	Json mixed(JsonType::Array);
	mixed.add(1).add("1").add(true).add(nullptr).add(Json{ {"a", 1} });
	EXPECT_EQ(mixed.indexOf("1"), 0);                        // the number matches first
	EXPECT_EQ(mixed.indexOf("true"), 2);
	EXPECT_EQ(mixed.indexOf("null"), 3);
	EXPECT_EQ(mixed.indexOf("{\"a\":1}"), 4);
	EXPECT_EQ(mixed.indexOf("false"), -1);

	EXPECT_EQ(array.slice(1, 3).toString(), "[1,2]");
	EXPECT_EQ(array.slice(2).toString(), "[2,3,4]");
	EXPECT_EQ(array.slice(0).toString(), "[0,1,2,3,4]");
	EXPECT_EQ(array.slice(4, 2).toString(), "[]");           // empty range
	EXPECT_EQ(array.slice(9).toString(), "[]");              // start beyond the end
	EXPECT_EQ(array.slice(1, 3).size(), 2);                  // slice() does not modify the source
	EXPECT_EQ(array.toString(), "[0,1,2,3,4]");

	EXPECT_TRUE(Json(5).first().isError());
	EXPECT_TRUE(Json(5).last().isError());
	EXPECT_TRUE(Json(JsonType::Array).first().isError());
	EXPECT_TRUE(Json(JsonType::Array).last().isError());
	EXPECT_EQ(Json(5).indexOf("5"), -1);
	EXPECT_EQ(Json(5).slice(0).size(), 0);
}

// =============================================================================
// 4. Mutation
// =============================================================================

TEST(TestApiCoverage, unnamed_add_only_appends_to_arrays) {
	Json array(JsonType::Array);
	array.add(1);                                  // template add(T)
	array.add(Json(2));                            // add(const Json&)
	array.add(Json("three"));                      // add(Json&&)
	array.add(Json{ {"four", 4} });                // add(Json&&) with object
	array.add({ "5", 6 });                         // add(initializer_list<Json>)
	EXPECT_EQ(array.toString(), "[1,2,\"three\",{\"four\":4},\"5\",6]");

	// On an object the unnamed forms are no-ops (a value without a key is
	// meaningless), and the document is not modified.
	Json object;
	object.add(1);
	object.add(Json(2));
	object.add(Json{ {"a", 1} });
	object.add({ 1, 2 });
	EXPECT_EQ(object.toString(), "{}");

	// On scalars everything is a no-op too.
	Json number(7);
	number.add(1);
	number.add("k", 2);
	EXPECT_EQ(number.toString(), "7");
}

TEST(TestApiCoverage, named_add_honours_the_key) {
	Json object;
	object.add("int", 1);
	object.add("double", 2.5);
	object.add("bool", true);
	object.add("null", nullptr);
	object.add("text", "value");
	object.add("std::string", std::string("value2"));
	object.add("array", Json(JsonType::Array).add({1, 2}));
	object.add("object", Json{ {"x", 1} });
	object.add("", "empty key");                   // empty key is honoured
	EXPECT_EQ(object.toString(),
		"{\"int\":1,\"double\":2.5,\"bool\":true,\"null\":null,\"text\":\"value\","
		"\"std::string\":\"value2\",\"array\":[1,2],\"object\":{\"x\":1},\"\":\"empty key\"}");
	EXPECT_EQ(object[""].toString(), "empty key");
	EXPECT_EQ(object.at("/").toString(), "empty key");

	// Duplicate keys are kept (the document is a list, not a map).
	object.add("dup", 1).add("dup", 2);
	EXPECT_EQ(object["dup"].toInt(), 2);           // keymap resolves the last one
	EXPECT_NE(object.toString().find("\"dup\":1,\"dup\":2"), std::string::npos);

	// On arrays the name is ignored and the value is appended.
	Json array(JsonType::Array);
	array.add("ignored", 5);
	EXPECT_EQ(array.toString(), "[5]");

	// Chained adds build a document fluently.
	Json chained;
	chained.add("a", 1).add("b", 2).add("c", 3);
	EXPECT_EQ(chained.toString(), "{\"a\":1,\"b\":2,\"c\":3}");
}

TEST(TestApiCoverage, push_front_back_and_pop_family) {
	Json array(JsonType::Array);
	array.push_back(2);
	array.push(3);                                 // alias for push_back
	array.push_front(1);
	array.push_front(0);
	EXPECT_EQ(array.toString(), "[0,1,2,3]");

	array.pop_back();
	EXPECT_EQ(array.toString(), "[0,1,2]");
	array.pop_front();
	EXPECT_EQ(array.toString(), "[1,2]");
	array.pop_back();                           // alias for pop_front/pop_back
	EXPECT_EQ(array.toString(), "[1]");
	// pop() removes the tail and returns the container, not the removed value.
	EXPECT_EQ(array.pop().toString(), "[]");
	EXPECT_TRUE(array.isEmpty());

	array.pop_back();
	EXPECT_TRUE(array.isEmpty());
	array.pop_front();                             // no-op on empty
	array.pop_back();                              // no-op on empty
	EXPECT_EQ(array.toString(), "[]");

	// Non-arrays ignore the whole family.
	Json object;
	object.push_back(1);
	object.push_front(1);
	object.pop_back();
	object.pop_front();
	object.pop();
	EXPECT_EQ(object.toString(), "{}");
}

TEST(TestApiCoverage, insert_positions) {
	Json array(JsonType::Array);
	array.add({ 1, 2, 3 });                        // size 3

	array.insert(0, 0);                            // head
	EXPECT_EQ(array.toString(), "[0,1,2,3]");

	array.insert(2, 99);                           // middle
	EXPECT_EQ(array.toString(), "[0,1,99,2,3]");  // size 5

	array.insert(5, 4);                            // index == size appends
	EXPECT_EQ(array.toString(), "[0,1,99,2,3,4]"); // size 6

	array.insert(7, 5);                            // index > size is a no-op
	EXPECT_EQ(array.toString(), "[0,1,99,2,3,4]");
	array.insert(100, 5);
	EXPECT_EQ(array.toString(), "[0,1,99,2,3,4]");

	array.insert(-1, 9);                           // negative indexes count from the end
	EXPECT_EQ(array.toString(), "[0,1,99,2,3,9,4]");  // inserted before the last element
	array.insert(-100, 9);                         // still negative -> no-op
	EXPECT_EQ(array.toString(), "[0,1,99,2,3,9,4]");

	// The result is chainable and the tail pointer stays consistent.
	array.insert(0, "x").insert(0, "y");
	EXPECT_EQ(array.toString(), "[\"y\",\"x\",0,1,99,2,3,9,4]");

	// Non-arrays ignore insert().
	Json object;
	object.insert(0, 1);
	EXPECT_EQ(object.toString(), "{}");

	// Inserting into an empty array: index 0 appends, index 1 is out of range.
	Json single(JsonType::Array);
	single.insert(0, 1);
	EXPECT_EQ(single.toString(), "[1]");
	Json single2(JsonType::Array);
	single2.insert(1, 1);
	EXPECT_EQ(single2.toString(), "[]");
	single2.insert(0, 2);
	EXPECT_EQ(single2.toString(), "[2]");
}

TEST(TestApiCoverage, remove_by_index_family) {
	Json array(JsonType::Array);
	array.add({ 0, 1, 2, 3, 4 });

	array.remove(0);
	EXPECT_EQ(array.toString(), "[1,2,3,4]");
	array.remove(3);
	EXPECT_EQ(array.toString(), "[1,2,3]");
	array.remove(1);
	EXPECT_EQ(array.toString(), "[1,3]");

	array.remove(5);                               // out of range: no-op
	array.remove(-1);                              // negative: no-op
	EXPECT_EQ(array.toString(), "[1,3]");

	array.removeFirst();
	EXPECT_EQ(array.toString(), "[3]");
	array.removeLast();
	EXPECT_EQ(array.toString(), "[]");
	array.removeFirst();                           // no-op on empty
	array.removeLast();                            // no-op on empty
	EXPECT_EQ(array.toString(), "[]");

	// Appending still works after the tail was removed (lastChild repair).
	array.add(7);
	EXPECT_EQ(array.toString(), "[7]");

	// Non-arrays ignore index removal.
	Json object;
	object.add("a", 1);
	object.remove(0);
	EXPECT_EQ(object.toString(), "{\"a\":1}");
}

TEST(TestApiCoverage, clear_and_reuse) {
	Json doc("{\"a\":1,\"b\":[1,2,3],\"c\":{\"d\":4}}");
	doc.clear();
	EXPECT_EQ(doc.toString(), "{}");
	EXPECT_TRUE(doc.isObject());

	doc.add("fresh", 1);
	EXPECT_EQ(doc.toString(), "{\"fresh\":1}");
	EXPECT_EQ(doc["fresh"].toInt(), 1);

	Json array(JsonType::Array);
	array.add({ 1, 2, 3 });
	array.clear();
	EXPECT_EQ(array.toString(), "[]");
	array.add(9);
	EXPECT_EQ(array.toString(), "[9]");

	// clear() on scalars is a no-op.
	Json number(5);
	number.clear();
	EXPECT_EQ(number.toInt(), 5);
}

TEST(TestApiCoverage, take_and_takes) {
	Json doc;
	doc.add("keep", 1).add("take", 2).add("tail", 3);
	EXPECT_EQ(doc.take("take").toInt(), 2);
	EXPECT_EQ(doc.toString(), "{\"keep\":1,\"tail\":3}");
	EXPECT_EQ(doc.take("keep").toInt(), 1);
	EXPECT_EQ(doc.toString(), "{\"tail\":3}");

	// Taking a missing key yields an Error node and leaves the document alone.
	Json taken = doc.take("missing");
	EXPECT_TRUE(taken.isError());
	EXPECT_EQ(doc.toString(), "{\"tail\":3}");

	// take(int) removes by index.
	Json array(JsonType::Array);
	array.add({ 10, 20, 30 });
	EXPECT_EQ(array.take(1).toInt(), 20);
	EXPECT_EQ(array.toString(), "[10,30]");
	EXPECT_EQ(array.take(0).toInt(), 10);
	EXPECT_EQ(array.toString(), "[30]");
	EXPECT_TRUE(array.take(5).isError());
	array.add(1);
	EXPECT_EQ(array.toString(), "[30,1]");

	// take(int) on a non-array is an Error node.
	EXPECT_TRUE(Json(5).take(0).isError());

	// takes(start, end) moves the slice out of the source.
	Json source(JsonType::Array);
	source.add({ 0, 1, 2, 3, 4, 5 });
	Json slice = source.takes(1, 4);
	EXPECT_EQ(slice.toString(), "[1,2,3]");
	EXPECT_EQ(source.toString(), "[0,4,5]");

	Json all(JsonType::Array);
	all.add({ 0, 1, 2 });
	EXPECT_EQ(all.takes(0).toString(), "[0,1,2]");
	EXPECT_EQ(all.toString(), "[]");

	// takes() on a non-array yields an empty array.
	EXPECT_EQ(Json(5).takes(0).size(), 0);
}

TEST(TestApiCoverage, extend_replaces_members_and_reorders) {
	Json base;
	base.add("a", 1).add("b", 2).add("c", 3);

	Json patch;
	patch.add("b", 20).add("d", 4);

	base.extend(patch);
	// Replaced members are removed first and appended afterwards, so the fresh
	// value moves to the end of the member order.
	EXPECT_EQ(base.toString(), "{\"a\":1,\"c\":3,\"b\":20,\"d\":4}");

	// Containers are moved, not copied, and stay intact.
	Json base2;
	base2.add("keep", true);
	Json patch2;
	patch2.add("obj", Json{ {"x", 1}, {"y", Json(JsonType::Array).add({1, 2})} });
	base2.extend(patch2);
	EXPECT_EQ(base2.toString(), "{\"keep\":true,\"obj\":{\"x\":1,\"y\":[1,2]}}");
	EXPECT_EQ(base2["obj"]["y"][1].toInt(), 2);

	// Type mismatches are no-ops.
	Json array(JsonType::Array);
	array.add(1);
	array.extend(Json{ {"x", 1} });
	EXPECT_EQ(array.toString(), "[1]");

	Json object;
	object.add("x", 1);
	object.extend(Json(JsonType::Array).add(1));
	EXPECT_EQ(object.toString(), "{\"x\":1}");

	// A member with the empty key can be extended too.
	Json emptyKey;
	emptyKey.add("", 1);
	Json emptyKeyPatch;
	emptyKeyPatch.add("", 2);
	emptyKey.extend(emptyKeyPatch);
	EXPECT_EQ(emptyKey.toString(), "{\"\":2}");
}

TEST(TestApiCoverage, concat_appends_children) {
	Json array(JsonType::Array);
	array.add(1).add(2);

	Json other(JsonType::Array);
	other.add(3).add(4);
	array.concat(other);
	EXPECT_EQ(array.toString(), "[1,2,3,4]");

	// Concat with an object appends the object's member values.
	Json object;
	object.add("a", 5).add("b", 6);
	array.concat(object);
	EXPECT_EQ(array.toString(), "[1,2,3,4,5,6]");

	// Concat with a scalar appends the scalar itself.
	array.concat(7);
	array.concat("text");
	EXPECT_EQ(array.toString(), "[1,2,3,4,5,6,7,\"text\"]");

	// The source document is unchanged.
	EXPECT_EQ(other.toString(), "[3,4]");
	EXPECT_EQ(object.toString(), "{\"a\":5,\"b\":6}");

	// Non-arrays ignore concat().
	Json notArray;
	notArray.add("a", 1);
	notArray.concat(Json(JsonType::Array).add(9));
	EXPECT_EQ(notArray.toString(), "{\"a\":1}");

	// concat() copies scalars (the argument is unaffected).
	Json scalarSource(42);
	Json target(JsonType::Array);
	target.concat(scalarSource);
	EXPECT_EQ(scalarSource.toInt(), 42);
	EXPECT_EQ(target.toString(), "[42]");
}

TEST(TestApiCoverage, remove_key_keeps_the_tree_consistent) {
	// Regression: a member removed while the preceding sibling is a container
	// used to corrupt the chain (dropped subtree + dangling link).
	Json doc;
	doc.add("a", 1);
	doc.add("b", Json{ {"x", 1}, {"y", 2} });
	doc.add("c", 3);
	doc.remove("c");
	EXPECT_EQ(doc.toString(), "{\"a\":1,\"b\":{\"x\":1,\"y\":2}}");

	// Regression: first child of a nested container.
	Json nested;
	nested.add("p", Json{ {"k", 1}, {"x", 2} });
	nested.add("q", 0);
	nested.add("k", 9);
	nested.remove("k");
	EXPECT_EQ(nested.toString(), "{\"p\":{\"x\":2},\"q\":0}");

	// Mixed container/scalar members, removal in several orders.
	Json mixed;
	mixed.add("a", 1);
	mixed.add("sub", Json{ {"one", 1} });
	mixed.add("b", 2);
	mixed.add("arr", Json(JsonType::Array).add({ 1, 2 }));
	mixed.add("c", 3);
	mixed.remove("b");
	EXPECT_EQ(mixed.toString(), "{\"a\":1,\"sub\":{\"one\":1},\"arr\":[1,2],\"c\":3}");
	mixed.remove("c");
	EXPECT_EQ(mixed.toString(), "{\"a\":1,\"sub\":{\"one\":1},\"arr\":[1,2]}");
	mixed.remove("a");
	EXPECT_EQ(mixed.toString(), "{\"sub\":{\"one\":1},\"arr\":[1,2]}");

	// Adding after removals must keep working (tail pointer is repaired).
	mixed.add("z", 26);
	EXPECT_EQ(mixed.toString(), "{\"sub\":{\"one\":1},\"arr\":[1,2],\"z\":26}");

	// Removal only applies to object members; array elements are name-less and
	// survive a deep removal pass.
	Json withArray("\"placeholder\"");
	withArray = Json("{\"list\":[1,2,3],\"k\":1}");
	withArray.remove("k");
	EXPECT_EQ(withArray.toString(), "{\"list\":[1,2,3]}");

	// Removing a non-existent key changes nothing.
	Json untouched("{\"a\":1}");
	untouched.remove("zzz");
	EXPECT_EQ(untouched.toString(), "{\"a\":1}");

	// remove() reaches nested members (historical, documented behaviour).
	Json deep("{\"outer\":{\"inner\":{\"target\":1,\"keep\":2}},\"target\":3}");
	deep.remove("target");
	EXPECT_EQ(deep.toString(), "{\"outer\":{\"inner\":{\"keep\":2}}}");

	// Removing the empty key removes empty-keyed members only.
	Json emptyKeys("{\"\":1,\"a\":2}");
	emptyKeys.remove("");
	EXPECT_EQ(emptyKeys.toString(), "{\"a\":2}");

	// Non-containers ignore remove(key).
	Json number(5);
	number.remove("a");
	EXPECT_EQ(number.toInt(), 5);
	Json array(JsonType::Array);
	array.add(1);
	array.remove("a");
	EXPECT_EQ(array.toString(), "[1]");
}

TEST(TestApiCoverage, remove_key_inside_arrays_and_deep_documents) {
	Json doc("{\"items\":[{\"name\":\"a\",\"drop\":1},{\"name\":\"b\",\"drop\":2}],\"drop\":3}");
	doc.remove("drop");
	EXPECT_EQ(doc.toString(), "{\"items\":[{\"name\":\"a\"},{\"name\":\"b\"}]}");

	// Repeated removals on the same document stay stable.
	Json repeated("{\"k\":0,\"sub\":{\"k\":1,\"deeper\":{\"k\":2}}}");
	repeated.remove("k");
	EXPECT_EQ(repeated.toString(), "{\"sub\":{\"deeper\":{}}}");
	repeated.add("k", 9);
	EXPECT_EQ(repeated["k"].toInt(), 9);
}

// =============================================================================
// 5. JSON Pointer, Merge Patch, JSON Patch
// =============================================================================

TEST(TestApiCoverage, json_pointer_lookup_cases) {
	Json doc;
	doc.add("plain", 1);
	doc.add("a/b", 2);
	doc.add("m~n", 3);
	doc.add("arr", Json(JsonType::Array).add({ Json{ {"deep", 4} }, 5 }));
	doc.add("", 6);

	EXPECT_EQ(doc.at("/plain").toInt(), 1);
	EXPECT_EQ(doc.at("/a~1b").toInt(), 2);          // ~1 -> '/'
	EXPECT_EQ(doc.at("/m~0n").toInt(), 3);          // ~0 -> '~'
	EXPECT_EQ(doc.at("/arr/0/deep").toInt(), 4);
	EXPECT_EQ(doc.at("/arr/1").toInt(), 5);
	EXPECT_EQ(doc.at("/").toInt(), 6);              // empty key
	EXPECT_TRUE(doc.at("").isObject());             // empty pointer: whole document
	EXPECT_EQ(doc.at(""), doc);

	// Failure modes.
	EXPECT_TRUE(doc.at("/missing").isError());
	EXPECT_TRUE(doc.at("plain").isError());         // must start with '/'
	EXPECT_TRUE(doc.at("/arr/01").isError());       // no leading zeros in indexes
	EXPECT_TRUE(doc.at("/arr/-").isError());        // '-' is not an index here
	EXPECT_TRUE(doc.at("/arr/x").isError());
	EXPECT_TRUE(doc.at("/arr/9").isError());
	EXPECT_TRUE(doc.at("/a~2b").isError());         // invalid escape
	EXPECT_TRUE(doc.at("/a~").isError());           // trailing '~'
	EXPECT_TRUE(doc.at("/plain/deeper").isError()); // traverses a scalar
	EXPECT_TRUE(Json(5).at("/x").isError());        // non-container root

	// at() returns a copy.
	Json value = doc.at("/plain");
	value.add("x", 1);
	EXPECT_EQ(doc.at("/plain").toInt(), 1);
}

TEST(TestApiCoverage, merge_patch_rfc7396_cases) {
	// The example from RFC 7396 section 3.
	Json target{
		{"title", "Goodbye!"},
		{"author", Json{ {"givenName", "John"}, {"familyName", "Doe"} }},
		{"tags", Json(JsonType::Array).add({ "example", "sample" })},
		{"content", "This will be unchanged"}
	};
	Json patch{
		{"title", "Hello!"},
		{"phoneNumber", "+01-123-456-7890"},
		{"author", Json{ {"familyName", nullptr} }},
		{"tags", Json(JsonType::Array).add({ "example" })}
	};
	target.mergePatch(patch);
	EXPECT_EQ(target.toString(),
		"{\"title\":\"Hello!\",\"author\":{\"givenName\":\"John\"},\"tags\":[\"example\"],"
		"\"content\":\"This will be unchanged\",\"phoneNumber\":\"+01-123-456-7890\"}");

	// A non-object patch replaces the target entirely.
	Json replaced("{\"a\":1}");
	replaced.mergePatch(Json(7));
	EXPECT_TRUE(replaced.isNumber());
	EXPECT_EQ(replaced.toInt(), 7);

	Json replacedByArray("{\"a\":1}");
	replacedByArray.mergePatch(Json(JsonType::Array).add({1, 2}));
	EXPECT_EQ(replacedByArray.toString(), "[1,2]");

	// A non-object target becomes an object when patched with an object.
	Json scalar(5);
	scalar.mergePatch(Json{ {"created", true} });
	EXPECT_EQ(scalar.toString(), "{\"created\":true}");

	// An empty patch changes nothing.
	Json stable("{\"a\":1}");
	stable.mergePatch(Json(JsonType::Object));
	EXPECT_EQ(stable.toString(), "{\"a\":1}");

	// null removes a member; a null for an absent member is a no-op.
	Json removal("{\"a\":1,\"b\":2}");
	removal.mergePatch(Json{ {"a", nullptr}, {"zz", nullptr} });
	EXPECT_EQ(removal.toString(), "{\"b\":2}");

	// Deep merging keeps untouched nested members.
	Json deep("{\"o\":{\"a\":1,\"b\":2},\"keep\":true}");
	Json nestedPatch{ {"b", 20}, {"c", 30} };
	deep.mergePatch(Json{ {"o", nestedPatch} });
	EXPECT_EQ(deep.toString(), "{\"o\":{\"a\":1,\"b\":20,\"c\":30},\"keep\":true}");

	// Arrays in the patch replace arrays in the target wholesale.
	Json arrays("{\"list\":[1,2,3]}");
	arrays.mergePatch(Json{ {"list", Json(JsonType::Array).add({9})} });
	EXPECT_EQ(arrays.toString(), "{\"list\":[9]}");

	// Merging with a copy of itself is stable.
	Json idempotent("{\"a\":[1,2],\"b\":{\"c\":true}}");
	idempotent.mergePatch(Json(idempotent));
	EXPECT_EQ(idempotent.toString(), "{\"a\":[1,2],\"b\":{\"c\":true}}");
}

TEST(TestApiCoverage, json_patch_rfc6902_operations) {
	Json base{
		{"foo", "bar"},
		{"numbers", Json(JsonType::Array).add({1, 2, 3})},
		{"nested", Json{ {"x", 1} }}
	};

	Json ops(JsonType::Array);
	ops.add(Json{ {"op", "add"}, {"path", "/numbers/-"}, {"value", 4} });
	ops.add(Json{ {"op", "replace"}, {"path", "/foo"}, {"value", "baz"} });
	ops.add(Json{ {"op", "add"}, {"path", "/nested/y"}, {"value", true} });
	ops.add(Json{ {"op", "copy"}, {"from", "/nested/y"}, {"path", "/copied"} });
	ops.add(Json{ {"op", "move"}, {"from", "/numbers/0"}, {"path", "/numbers/2"} });
	ops.add(Json{ {"op", "test"}, {"path", "/nested/x"}, {"value", 1} });
	ops.add(Json{ {"op", "remove"}, {"path", "/nested/x"} });

	std::string err;
	Json patched = base.applyPatch(ops, err);
	ASSERT_FALSE(patched.isError()) << err;
	EXPECT_TRUE(err.empty());
	EXPECT_EQ(patched.toString(), "{\"foo\":\"baz\",\"numbers\":[2,3,1,4],\"nested\":{\"y\":true},\"copied\":true}");

	// The receiver is untouched: applyPatch() works on a copy.
	EXPECT_EQ(base.toString(), "{\"foo\":\"bar\",\"numbers\":[1,2,3],\"nested\":{\"x\":1}}");

	// "add" on an existing object member replaces it in place.
	Json replaceExisting("{\"a\":1,\"b\":2}");
	Json addOps(JsonType::Array);
	addOps.add(Json{ {"op", "add"}, {"path", "/a"}, {"value", 99} });
	std::string err2;
	Json replaced = replaceExisting.applyPatch(addOps, err2);
	ASSERT_FALSE(replaced.isError()) << err2;
	EXPECT_EQ(replaced.toString(), "{\"a\":99,\"b\":2}");

	// Replacing the whole document through the empty pointer.
	Json whole("{\"a\":1}");
	Json rootOps(JsonType::Array);
	rootOps.add(Json{ {"op", "replace"}, {"path", ""}, {"value", Json{ {"b", 2} }} });
	std::string err3;
	Json replacedRoot = whole.applyPatch(rootOps, err3);
	ASSERT_FALSE(replacedRoot.isError()) << err3;
	EXPECT_EQ(replacedRoot.toString(), "{\"b\":2}");

	// Removing the last array element and inserting at a valid index.
	Json arrOps(JsonType::Array);
	arrOps.add(Json{ {"op", "remove"}, {"path", "/list/0"} });
	arrOps.add(Json{ {"op", "add"}, {"path", "/list/0"}, {"value", 42} });
	std::string err4;
	Json arr = Json("{\"list\":[7,8]}").applyPatch(arrOps, err4);
	ASSERT_FALSE(arr.isError()) << err4;
	EXPECT_EQ(arr.toString(), "{\"list\":[42,8]}");
}

TEST(TestApiCoverage, json_patch_reports_every_failure_mode) {
	auto expectFailure = [](const Json& document, const Json& ops, const char* fragment) {
		std::string err;
		Json result = document.applyPatch(ops, err);
		EXPECT_TRUE(result.isError()) << "expected failure for: " << ops.toString();
		EXPECT_NE(err.find(fragment), std::string::npos) << "actual error: " << err;
	};

	Json document("{\"foo\":\"bar\",\"list\":[1,2]}");

	expectFailure(document, Json{ {"op", "add"} }, "must be an array");
	expectFailure(document, Json(JsonType::Array).add(5), "each JSON Patch operation must be an object");
	expectFailure(document, Json(JsonType::Array).add(Json{ {"path", "/foo"} }), "missing string field 'op'");
	expectFailure(document, Json(JsonType::Array).add(Json{ {"op", "add"} }), "missing string field 'path'");
	expectFailure(document, Json(JsonType::Array).add(Json{ {"op", "add"}, {"path", "foo"}, {"value", 1} }), "must start with '/'");
	expectFailure(document, Json(JsonType::Array).add(Json{ {"op", "add"}, {"path", "/foo"} }), "requires field 'value'");
	expectFailure(document, Json(JsonType::Array).add(Json{ {"op", "replace"}, {"path", "/foo"} }), "requires field 'value'");
	expectFailure(document, Json(JsonType::Array).add(Json{ {"op", "remove"}, {"path", ""} }), "removing the document root");
	expectFailure(document, Json(JsonType::Array).add(Json{ {"op", "test"}, {"path", "/foo"}, {"value", "baz"} }), "test operation failed");
	expectFailure(document, Json(JsonType::Array).add(Json{ {"op", "frobnicate"}, {"path", "/foo"} }), "unsupported JSON Patch operation");
	expectFailure(document, Json(JsonType::Array).add(Json{ {"op", "move"}, {"path", "/x"} }), "requires string field 'from'");
	expectFailure(document, Json(JsonType::Array).add(Json{ {"op", "move"}, {"from", "/foo"}, {"path", "/foo/bar"} }), "cannot be inside source path");
	expectFailure(document, Json(JsonType::Array).add(Json{ {"op", "add"}, {"path", "/list/x"}, {"value", 1} }), "invalid array index");
	expectFailure(document, Json(JsonType::Array).add(Json{ {"op", "add"}, {"path", "/list/9"}, {"value", 1} }), "out of bounds");
	expectFailure(document, Json(JsonType::Array).add(Json{ {"op", "remove"}, {"path", "/nope"} }), "does not exist");
	expectFailure(document, Json(JsonType::Array).add(Json{ {"op", "replace"}, {"path", "/foo/deeper"}, {"value", 1} }), "parent is not a container");
	expectFailure(document, Json(JsonType::Array).add(Json{ {"op", "copy"}, {"from", "/nope"}, {"path", "/x"} }), "does not exist");
	expectFailure(document, Json(JsonType::Array).add(Json{ {"op", "add"}, {"path", "/a~2b"}, {"value", 1} }), "invalid JSON Pointer escape");
}

// =============================================================================
// 6. Serialization
// =============================================================================

TEST(TestApiCoverage, compact_serialization_forms) {
	EXPECT_EQ(Json(JsonType::Object).toString(), "{}");
	EXPECT_EQ(Json(JsonType::Array).toString(), "[]");

	Json doc;
	doc.add("null", nullptr);
	doc.add("true", true);
	doc.add("false", false);
	doc.add("zero", 0);
	doc.add("int", -12);
	doc.add("double", 1.5);
	doc.add("string", "text");
	doc.add("emptyString", "");
	doc.add("emptyArray", Json(JsonType::Array));
	doc.add("emptyObject", Json(JsonType::Object));
	doc.add("nested", Json(JsonType::Array).add({ Json(JsonType::Object).add("x", 1) }));
	EXPECT_EQ(doc.toString(),
		"{\"null\":null,\"true\":true,\"false\":false,\"zero\":0,\"int\":-12,\"double\":1.5,"
		"\"string\":\"text\",\"emptyString\":\"\",\"emptyArray\":[],\"emptyObject\":{},"
		"\"nested\":[{\"x\":1}]}");

	// Top-level strings serialize as their raw value (historical behaviour).
	EXPECT_EQ(Json("plain").toString(), "plain");
	EXPECT_EQ(Json(JsonType::Array).add("quoted\"inside").toString(), "[\"quoted\\\"inside\"]");
}

TEST(TestApiCoverage, pretty_serialization_forms) {
	Json doc;
	doc.add("a", 1);
	doc.add("b", Json(JsonType::Array).add({ true, "x" }));
	doc.add("c", Json(JsonType::Object));

	EXPECT_EQ(doc.toString(2),
		"{\n  \"a\": 1,\n  \"b\": [\n    true,\n    \"x\"\n  ],\n  \"c\": {}\n}");
	EXPECT_EQ(doc.toString(4),
		"{\n    \"a\": 1,\n    \"b\": [\n        true,\n        \"x\"\n    ],\n    \"c\": {}\n}");
	EXPECT_EQ(doc.toString(1),
		"{\n \"a\": 1,\n \"b\": [\n  true,\n  \"x\"\n ],\n \"c\": {}\n}");

	// indent <= 0 falls back to the compact form.
	EXPECT_EQ(doc.toString(0), doc.toString());
	EXPECT_EQ(doc.toString(-3), doc.toString());

	// Empty containers and scalars.
	EXPECT_EQ(Json(JsonType::Object).toString(2), "{}");
	EXPECT_EQ(Json(JsonType::Array).toString(2), "[]");
	EXPECT_EQ(Json(1).toString(2), "1");
	std::string parseError;
	EXPECT_EQ(Json::ParseJson("{bad", parseError).toString(2), "");
}

TEST(TestApiCoverage, escape_rules_in_keys_and_values) {
	Json doc;
	doc.add("quote\"key", "value\"quoted");
	doc.add("back\\slash", "a\\b");
	doc.add("newline\nkey", "line1\nline2");
	doc.add("tab\tkey", "col1\tcol2");
	doc.add("cr\rkey", "x\ry");
	doc.add("bs\bkey", "x\by");
	doc.add("ff\fkey", "x\fy");
	doc.add("control\x01key", "a\x01z");
	doc.add("del\x1fkey", "a\x1fz");

	const std::string out = doc.toString();
	EXPECT_NE(out.find("\"quote\\\"key\":\"value\\\"quoted\""), std::string::npos);
	EXPECT_NE(out.find("\"back\\\\slash\":\"a\\\\b\""), std::string::npos);
	EXPECT_NE(out.find("\"newline\\nkey\":\"line1\\nline2\""), std::string::npos);
	EXPECT_NE(out.find("\"tab\\tkey\":\"col1\\tcol2\""), std::string::npos);
	EXPECT_NE(out.find("\"cr\\rkey\":\"x\\ry\""), std::string::npos);
	EXPECT_NE(out.find("\"bs\\bkey\":\"x\\by\""), std::string::npos);
	EXPECT_NE(out.find("\"ff\\fkey\":\"x\\fy\""), std::string::npos);
	EXPECT_NE(out.find("\"control\\u0001key\":\"a\\u0001z\""), std::string::npos);
	EXPECT_NE(out.find("\"del\\u001fkey\":\"a\\u001fz\""), std::string::npos);

	// Everything round-trips through the parser.
	Json reparsed(out);
	ASSERT_TRUE(reparsed.isObject());
	EXPECT_EQ(reparsed["quote\"key"].toString(), "value\"quoted");
	EXPECT_EQ(reparsed["back\\slash"].toString(), "a\\b");
	EXPECT_EQ(reparsed["newline\nkey"].toString(), "line1\nline2");
	EXPECT_EQ(reparsed["control\x01key"].toString(), "a\x01z");

	// Non-ASCII UTF-8 passes through unescaped.
	Json utf8;
	utf8.add("k", "\xE4\xB8\xAD\xE6\x96\x87");     // 中文
	EXPECT_EQ(utf8.toString(), "{\"k\":\"\xE4\xB8\xAD\xE6\x96\x87\"}");
	EXPECT_EQ(Json(utf8.toString())["k"].toString(), "\xE4\xB8\xAD\xE6\x96\x87");
}

TEST(TestApiCoverage, number_serialization_rules) {
	EXPECT_EQ(Json(0).toString(), "0");
	EXPECT_EQ(Json(-0.0).toString(), "-0");
	EXPECT_EQ(Json(1).toString(), "1");
	EXPECT_EQ(Json(-1).toString(), "-1");
	EXPECT_EQ(Json(1.0).toString(), "1");
	EXPECT_EQ(Json(-1.5).toString(), "-1.5");
	EXPECT_EQ(Json(0.1).toString(), "0.1");
	EXPECT_EQ(Json(1e10).toString(), "10000000000");
	EXPECT_EQ(Json(1e-7).toString(), "1e-07");
	EXPECT_EQ(Json(1.0e300).toString(), "1e+300");

	// Non-finite values are written as null (RFC 8259 forbids NaN/Inf).
	EXPECT_EQ(Json(std::nan("")).toString(), "null");
	EXPECT_EQ(Json(HUGE_VAL).toString(), "null");
	EXPECT_EQ(Json(-HUGE_VAL).toString(), "null");

	// Round-trip stability for values parsed from text.
	const char* numbers[] = {
		"0", "-0", "1", "-1", "9007199254740992", "1e308", "-1e308",
		"5e-324", "0.1", "2.5", "123456789.123456789", "1e-7", "1E+5"
	};
	for (const char* text : numbers) {
		std::string err;
		Json parsed = Json::ParseJsonStrict(text, err);
		ASSERT_FALSE(parsed.isError()) << text << " -> " << err;
		ASSERT_TRUE(parsed.isNumber()) << text;
		Json again = Json::ParseJsonStrict(parsed.toString(), err);
		ASSERT_FALSE(again.isError()) << parsed.toString() << " -> " << err;
		ASSERT_TRUE(again.isNumber()) << parsed.toString();
		EXPECT_EQ(again.toString(), parsed.toString()) << text;
		EXPECT_DOUBLE_EQ(again.toDouble(), parsed.toDouble()) << text;
	}
}

TEST(TestApiCoverage, stream_dump_and_operator) {
	Json doc;
	doc.add("a", 1);
	doc.add("b", Json(JsonType::Array).add({ true, "x" }));

	std::ostringstream compact;
	doc.dump(compact);
	EXPECT_EQ(compact.str(), "{\"a\":1,\"b\":[true,\"x\"]}");

	std::ostringstream compactExplicit;
	doc.dump(compactExplicit, 0);
	EXPECT_EQ(compactExplicit.str(), doc.toString());

	std::ostringstream pretty;
	doc.dump(pretty, 2);
	EXPECT_EQ(pretty.str(), doc.toString(2));

	std::ostringstream streamed;
	streamed << doc;
	EXPECT_EQ(streamed.str(), doc.toString());

	std::ostringstream streamedPretty;
	streamedPretty << Json("{\"nested\":[1]}{x}");   // falls back to a string value
	EXPECT_EQ(streamedPretty.str(), "{\"nested\":[1]}{x}");
}

TEST(TestApiCoverage, round_trip_of_a_realistic_document) {
	const std::string source =
		"{\"id\":42,\"name\":\"ZJSON\",\"tags\":[\"json\",\"cpp\"],"
		"\"metrics\":{\"parse\":true,\"ratio\":0.5,\"limit\":null},"
		"\"matrix\":[[1,2],[3,4]],\"escaped\":\"a\\\"b\\\\c\\nd\"}";

	std::string err;
	Json parsed = Json::ParseJsonStrict(source, err);
	ASSERT_FALSE(parsed.isError()) << err;

	const std::string once = parsed.toString();
	Json reparsed(once);
	ASSERT_TRUE(reparsed.isObject());
	EXPECT_EQ(reparsed.toString(), once);
	EXPECT_EQ(reparsed, parsed);
	EXPECT_EQ(reparsed["matrix"][1][0].toInt(), 3);
	EXPECT_EQ(reparsed["escaped"].toString(), "a\"b\\c\nd");

	// Copy, move and re-serialize keep the exact same text.
	Json copied = parsed;
	Json moved = std::move(copied);
	EXPECT_EQ(moved.toString(), once);

	// Pretty printing and compact printing parse back to the same document.
	EXPECT_EQ(Json(parsed.toString(4)), parsed);
}

// =============================================================================
// 7. Iterators
// =============================================================================

TEST(TestApiCoverage, iterator_range_and_structured_bindings) {
	Json doc;
	doc.add("alpha", 1);
	doc.add("beta", 2);
	doc.add("gamma", Json(JsonType::Array).add({3}));

	std::string keys;
	int sum = 0;
	for (auto& entry : doc) {
		keys += entry.key();
		if (entry.value().isNumber())
			sum += entry.value().toInt();
	}
	EXPECT_EQ(keys, "alphabetagamma");
	EXPECT_EQ(sum, 3);

	std::string boundKeys;
	int boundSum = 0;
	for (auto& [key, value] : doc) {
		boundKeys += key;
		if (value.isNumber())
			boundSum += value.toInt();
	}
	EXPECT_EQ(boundKeys, "alphabetagamma");
	EXPECT_EQ(boundSum, 3);

	// Iteration order is insertion order, and the iterator walks the members.
	int count = 0;
	for (auto it = doc.begin(); it != doc.end(); ++it)
		++count;
	EXPECT_EQ(count, 3);

	// The entry references the live node: writing through it is visible.
	for (auto& [key, value] : doc) {
		if (key == "alpha")
			value = Json(100);
	}
	EXPECT_EQ(doc["alpha"].toInt(), 100);
}

TEST(TestApiCoverage, const_iterator_and_cbegin_cend) {
	const Json doc("{\"a\":1,\"b\":2}");

	int viaBeginEnd = 0;
	for (auto it = doc.begin(); it != doc.end(); ++it)
		viaBeginEnd += it.value().toInt();
	EXPECT_EQ(viaBeginEnd, 3);

	int viaCbeginCend = 0;
	for (auto it = doc.cbegin(); it != doc.cend(); ++it) {
		EXPECT_FALSE(it.key().empty());
		viaCbeginCend += it.value().toInt();
	}
	EXPECT_EQ(viaCbeginCend, 3);

	int viaAuto = 0;
	for (const auto& [key, value] : doc) {
		EXPECT_FALSE(key.empty());
		viaAuto += value.toInt();
	}
	EXPECT_EQ(viaAuto, 3);
}

TEST(TestApiCoverage, iterator_tuple_protocol_and_traits) {
	Json doc;
	doc.add("key", 1);

	ASSERT_TRUE((std::is_same<std::iterator_traits<Json::iterator>::value_type, JsonEntry>::value));
	ASSERT_TRUE((std::is_same<std::iterator_traits<Json::const_iterator>::value_type, JsonConstEntry>::value));
	ASSERT_EQ((std::tuple_size<JsonEntry>::value), 2u);
	ASSERT_EQ((std::tuple_size<JsonConstEntry>::value), 2u);
	ASSERT_TRUE((std::is_same<std::tuple_element<1, JsonEntry>::type, Json>::value));

	Json::iterator it = doc.begin();
	EXPECT_EQ(get<0>(*it), "key");                 // ADL finds ZJSON::get
	EXPECT_EQ(get<1>(*it).toInt(), 1);
	EXPECT_EQ(it.key(), "key");
	EXPECT_EQ(it.value().toInt(), 1);
	EXPECT_EQ((*it).key(), "key");
	EXPECT_EQ(it->value().toInt(), 1);

	// Post-increment returns the previous position.
	Json::iterator before = it++;
	EXPECT_EQ(before.key(), "key");
	EXPECT_TRUE(it == doc.end());

	// Const entries expose the same protocol.
	const Json& cdoc = doc;
	Json::const_iterator cit = cdoc.cbegin();
	EXPECT_EQ(get<0>(*cit), "key");
	EXPECT_EQ(get<1>(*cit).toInt(), 1);
	EXPECT_EQ(cit->key(), "key");
}

TEST(TestApiCoverage, iterator_on_empty_and_non_container_nodes) {
	Json emptyObject;
	EXPECT_TRUE(emptyObject.begin() == emptyObject.end());

	Json emptyArray(JsonType::Array);
	EXPECT_TRUE(emptyArray.begin() == emptyArray.end());

	Json scalar(5);
	EXPECT_TRUE(scalar.begin() == scalar.end());

	// Array elements have no key.
	Json array(JsonType::Array);
	array.add(1).add(2);
	for (auto& [key, value] : array) {
		EXPECT_TRUE(key.empty());
		EXPECT_TRUE(value.isNumber());
	}

	// Two iterators over different documents never compare equal.
	Json a(JsonType::Array);
	a.add(1);
	Json b(JsonType::Array);
	b.add(1);
	EXPECT_FALSE(a.begin() == b.begin());
}

// =============================================================================
// 8. Equality
// =============================================================================

TEST(TestApiCoverage, equality_matrix) {
	// Same type and value.
	EXPECT_TRUE(Json(1) == Json(1));
	EXPECT_FALSE(Json(1) == Json(2));
	EXPECT_TRUE(Json(1.5) == Json(1.5));
	EXPECT_TRUE(Json("a") == Json("a"));
	EXPECT_FALSE(Json("a") == Json("b"));
	EXPECT_TRUE(Json(true) == Json(true));
	EXPECT_FALSE(Json(true) == Json(false));
	EXPECT_TRUE(Json(nullptr) == Json(nullptr));
	EXPECT_TRUE(Json(1) == Json(1.0));

	// Different types are never equal, even for "similar" values.
	EXPECT_FALSE(Json(1) == Json("1"));
	EXPECT_FALSE(Json(1) == Json(true));
	EXPECT_FALSE(Json(nullptr) == Json(false));
	EXPECT_FALSE(Json(JsonType::Object) == Json(JsonType::Array));

	// Objects compare member sets, ignoring order.
	Json lhs{ {"a", 1}, {"b", Json(JsonType::Array).add({2, 3})} };
	Json rhs{ {"b", Json(JsonType::Array).add({2, 3})}, {"a", 1} };
	EXPECT_TRUE(lhs == rhs);
	EXPECT_FALSE(lhs != rhs);

	// Extra or different members break equality; values matter, not just keys.
	Json extra = rhs;
	extra.add("c", 1);
	EXPECT_FALSE(lhs == extra);
	Json different = rhs;
	different.add("a", 2);                     // duplicate key with a new value
	EXPECT_FALSE(lhs == different);

	// Nested containers are compared recursively.
	Json nestedA("{\"o\":{\"arr\":[1,{\"deep\":true}]}}");
	Json nestedB("{\"o\":{\"arr\":[1,{\"deep\":true}]}}");
	Json nestedC("{\"o\":{\"arr\":[1,{\"deep\":false}]}}");
	EXPECT_TRUE(nestedA == nestedB);
	EXPECT_FALSE(nestedA == nestedC);

	// Arrays are order sensitive.
	Json arrA(JsonType::Array);
	arrA.add({ 1, 2, 3 });
	Json arrB(JsonType::Array);
	arrB.add({ 3, 2, 1 });
	Json arrC(JsonType::Array);
	arrC.add({ 1, 2, 3 });
	EXPECT_FALSE(arrA == arrB);
	EXPECT_TRUE(arrA == arrC);
	EXPECT_TRUE(arrA != arrB);

	// Different lengths are never equal.
	Json shorter(JsonType::Array);
	shorter.add({ 1, 2 });
	EXPECT_FALSE(arrA == shorter);

	// Error nodes compare equal to each other.
	std::string err;
	Json errorA = Json::ParseJson("{bad", err);
	Json errorB = Json::ParseJson("{bad", err);
	EXPECT_TRUE(errorA == errorB);
	EXPECT_FALSE(errorA == Json(1));

	// A copy equals its source, and inequality also round-trips.
	Json copy = nestedA;
	EXPECT_TRUE(copy == nestedA);
	EXPECT_FALSE(copy != nestedA);
}

// =============================================================================
// 9. Lifecycle, robustness and growth smoke tests
// =============================================================================

TEST(TestApiCoverage, wide_object_and_large_array_operations) {
	Json wide;
	for (int i = 0; i < 1500; ++i)
		wide.add("k" + std::to_string(i), i);
	EXPECT_EQ(wide.size(), -1);
	EXPECT_TRUE(wide.contains("k0"));
	EXPECT_EQ(wide["k1499"].toInt(), 1499);
	EXPECT_EQ(wide.at("/k750").toInt(), 750);

	Json wideCopy = wide;                     // deep copy of 1500 members
	EXPECT_EQ(wideCopy["k1499"].toInt(), 1499);
	wideCopy.remove("k0");
	EXPECT_FALSE(wideCopy.contains("k0"));
	EXPECT_TRUE(wide.contains("k0"));         // source untouched

	Json large(JsonType::Array);
	for (int i = 0; i < 2000; ++i)
		large.add(i);
	EXPECT_EQ(large.size(), 2000);
	EXPECT_EQ(large[1999].toInt(), 1999);
	EXPECT_EQ(large.take(1999).toInt(), 1999);
	EXPECT_EQ(large.size(), 1999);
	large.remove(0);
	EXPECT_EQ(large[0].toInt(), 1);
	EXPECT_EQ(large.size(), 1998);
	EXPECT_EQ(large.indexOf("1000"), 999);
}

TEST(TestApiCoverage, deeply_nested_documents_within_the_limit) {
	std::string text;
	const int depth = 100;
	for (int i = 0; i < depth; ++i)
		text += (i % 2 == 0) ? "[" : "{\"k\":";
	text += "1";
	for (int i = depth - 1; i >= 0; --i)      // closers mirror the openers in reverse
		text += (i % 2 == 0) ? "]" : "}";

	std::string err;
	Json parsed = Json::ParseJsonStrict(text, err);
	ASSERT_FALSE(parsed.isError()) << err;

	// Deep copy, serialize and destroy a deep document without recursion issues
	// (the traversal in remove() is iterative).
	Json copy = parsed;
	EXPECT_EQ(copy.toString(), parsed.toString());
	EXPECT_EQ(Json(parsed.toString(2)), parsed);
	copy.remove("k");
	EXPECT_FALSE(copy.isError());
}

TEST(TestApiCoverage, repeated_lifecycle_does_not_leak_unboundedly) {
	// A representative workload: build, copy, mutate, serialize, parse, patch,
	// take, extend, clear - i.e. every ownership-transferring path.
	auto workload = []() {
		for (int round = 0; round < 25; ++round) {
			Json doc;
			Json items(JsonType::Array);
			for (int i = 0; i < 40; ++i)
				items.add(Json{ {"id", i}, {"name", "item_" + std::to_string(i)}, {"ok", i % 2 == 0} });
			doc.add("items", items);
			doc.add("round", round);

			Json copy = doc;
			copy.remove("round");
			copy.add("extra", 1.5);
			copy.add("nested", Json{ {"deep", Json(JsonType::Array).add({1, 2, 3})} });

			const std::string text = copy.toString(2);
			Json reparsed(text);
			doc.mergePatch(reparsed);

			Json taken = doc.take("items");
			doc.extend(taken);

			Json array(JsonType::Array);
			array.add({ 1, 2, 3, 4, 5 });
			array.push_front(0);
			array.insert(3, 99);
			Json slice = array.takes(1, 3);
			array.concat(slice);
			array.remove(0);
			array.clear();
		}
	};

	// Warm-up repetitions absorb one-time allocations (slab growth, string
	// capacity, gtest laziness). Steady state must then be allocation-stable:
	// if any path leaked nodes, keys or keymaps this test would diverge.
	workload();
	workload();
	workload();

	const AllocSnapshot before = allocationSnapshot();
	workload();
	const size_t first = allocationsBetween(before);

	const AllocSnapshot beforeSecond = allocationSnapshot();
	workload();
	const size_t second = allocationsBetween(beforeSecond);

	EXPECT_GT(first, 0u) << "the workload should allocate";
	EXPECT_EQ(first, second) << "allocation count must be stable once warmed up";
}

TEST(TestApiCoverage, document_lifecycle_allocations_reach_steady_state) {
	// Creating, mutating, copying, patching and destroying documents must not
	// accumulate allocations: after warm-up the same workload allocates the exact
	// same number of blocks every time, which is what rules out a per-document
	// leak of keymaps, strings, arena buffers or node bookkeeping.
	auto workload = []() {
		for (int round = 0; round < 5; ++round) {
			Json doc;
			for (int i = 0; i < 100; ++i)
				doc.add("k" + std::to_string(i), Json{ {"a", i}, {"b", std::string(48, 'x')} });
			doc["k99"];                       // materialize keys via the keymap
			doc.remove("k50");
			Json copy = doc;
			doc.mergePatch(copy);
			std::string text = doc.toString();
			Json reparsed(text);
			doc.extend(reparsed);
		}
	};

	workload();
	workload();
	workload();

	const AllocSnapshot before = allocationSnapshot();
	workload();
	const size_t first = allocationsBetween(before);

	const AllocSnapshot beforeSecond = allocationSnapshot();
	workload();
	const size_t second = allocationsBetween(beforeSecond);

	EXPECT_GT(first, 0u) << "the workload should allocate";
	EXPECT_EQ(first, second) << "allocation count must be stable once warmed up";
}

// ---------------------------------------------------------------------------
// Cross-thread ownership. The node pool is process-lifetime and shared by the
// threads of the module, so a document may be built on one thread and destroyed
// on another - even after the building thread has exited. Before the pool became
// process-lifetime (thread_local pool + destructor) this sequence was a
// use-after-free: the thread's exit released its slabs while the document and the
// blocks parked in other threads' free lists still pointed at them.
// ---------------------------------------------------------------------------

TEST(TestApiCoverage, document_survives_owner_thread_exit_and_is_freed_elsewhere) {
	Json document;
	std::thread producer([&document]() {
		Json local;
		for (int i = 0; i < 500; ++i)
			local.add("k" + std::to_string(i), Json{ {"a", i}, {"b", std::string(32, 'x')} });
		local["k0"];                      // materialize the key index
		document = std::move(local);      // hand the document to this thread, then exit
	});
	producer.join();                      // thread exit must not invalidate the document

	// Reading (and later freeing) nodes that were allocated on the exited thread.
	EXPECT_EQ(document["k499"]["a"].toInt(), 499);
	EXPECT_EQ(document["k499"]["b"].toString(), std::string(32, 'x'));
	EXPECT_FALSE(document.toString().empty());
	EXPECT_TRUE(document.contains("k250"));

	// Freeing everything back into the shared pool, then reusing those blocks for
	// brand new nodes: recycled blocks must point at live memory.
	document.clear();
	EXPECT_EQ(document.toString(), "{}");

	Json fresh;
	for (int i = 0; i < 500; ++i)
		fresh.add("f" + std::to_string(i), i);
	EXPECT_EQ(fresh["f499"].toInt(), 499);
	EXPECT_EQ(fresh.getAllKeys().size(), 500);
}

TEST(TestApiCoverage, concurrent_allocation_and_deallocation_stays_consistent) {
	const int threadCount = 4;
	std::vector<Json> documents(static_cast<size_t>(threadCount));
	std::vector<std::thread> workers;
	workers.reserve(static_cast<size_t>(threadCount));

	for (int t = 0; t < threadCount; ++t) {
		workers.emplace_back([&documents, t]() {
			Json doc;
			for (int i = 0; i < 200; ++i)
				doc.add("k" + std::to_string(i), Json{ {"thread", t}, {"index", i} });
			doc["k199"];                  // build the key index
			documents[static_cast<size_t>(t)] = std::move(doc);
		});
	}
	for (std::thread& worker : workers)
		worker.join();

	for (int t = 0; t < threadCount; ++t) {
		const Json& doc = documents[static_cast<size_t>(t)];
		EXPECT_EQ(doc["k199"]["thread"].toInt(), t);
		EXPECT_EQ(doc["k199"]["index"].toInt(), 199);
		EXPECT_EQ(doc.getAllKeys().size(), 200);
	}

	// Deallocation happens on this thread for nodes built all over the place.
	for (Json& doc : documents)
		doc.clear();
	for (const Json& doc : documents)
		EXPECT_EQ(doc.toString(), "{}");
}

TEST(TestApiCoverage, document_can_be_mutated_on_another_thread_than_it_was_built) {
	// Ownership may cross thread boundaries more than once: nodes allocated on the
	// producer thread are unlinked, re-linked and finally released on other threads,
	// so the shared pool must keep handing back live blocks in both directions.
	Json document;
	std::thread producer([&document]() {
		Json local;
		for (int i = 0; i < 300; ++i)
			local.add("k" + std::to_string(i), Json{ {"i", i} });
		document = std::move(local);
	});
	producer.join();

	Json consumer;
	std::thread worker([&document, &consumer]() {
		document.remove("k0");                 // frees nodes that live in the producer's slabs
		document.add("added", true);
		document.add("nested", Json{ {"deep", Json(JsonType::Array).add({1, 2, 3})} });
		EXPECT_EQ(document["k299"]["i"].toInt(), 299);
		consumer = std::move(document);        // hand the document on once more
	});
	worker.join();

	EXPECT_FALSE(consumer.contains("k0"));
	EXPECT_TRUE(consumer["added"].isTrue());
	EXPECT_EQ(consumer["nested"]["deep"][2].toInt(), 3);
	EXPECT_EQ(consumer["k299"]["i"].toInt(), 299);

	// Final release on yet another owner (this thread).
	consumer.clear();
	EXPECT_EQ(consumer.toString(), "{}");
}
