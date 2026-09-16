// =============================================================================
//  ZJSON :: exception-safety suite (fault injection via global operator new)
//
//  R2 turned StoredString into a tagged union, which moved the tag flip in
//  strRef() before the possibly-throwing materialization.  That regression
//  (found 2026-09-15 by fault injection) left the object in a "tag says owned
//  but no string was constructed" state after std::bad_alloc - undefined
//  behaviour on the next destructor call, and a bogus string on the next read.
//
//  The fix materializes into a local std::string first (the only throwing
//  step), then flips the tag and moves the local in (noexcept).  These tests
//  arm a fail-on-Nth-allocation counter right before the call and verify that
//  a failed materialization leaves the document exactly as it was: still
//  borrowing, still readable, still serializable, still destructible.
//
//  The injection works by replacing the global operator new/delete family with
//  malloc/free plus a countdown.  This is only valid because this translation
//  unit is its own test binary: nothing here depends on allocations made
//  between arming the counter and the call under test.
// =============================================================================
#include "gtest/gtest.h"
#include "../src/zjson.hpp"

#include <cstdlib>
#include <new>
#include <string>

using namespace ZJSON;

namespace {

// -1 disabled; N = the Nth allocation from now on throws std::bad_alloc
// (the counter self-disarms when it fires).
int g_allocFailCountdown = -1;

}  // namespace

void* operator new(std::size_t size) {
	if (g_allocFailCountdown > 0 && --g_allocFailCountdown == 0) {
		g_allocFailCountdown = -1;
		throw std::bad_alloc();
	}
	if (void* p = std::malloc(size))
		return p;
	throw std::bad_alloc();
}
void* operator new[](std::size_t size) {
	if (g_allocFailCountdown > 0 && --g_allocFailCountdown == 0) {
		g_allocFailCountdown = -1;
		throw std::bad_alloc();
	}
	if (void* p = std::malloc(size))
		return p;
	throw std::bad_alloc();
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }

namespace {

// A key longer than any SSO buffer (15 chars on libc++/libstdc++), so that
// materializing it into a std::string performs exactly one allocation.
std::string longKey() {
	return std::string(80, 'K');
}

std::string sourceWithLongKey() {
	return "{\"" + longKey() + "\":1,\"other\":\"payload\"}";
}

}  // namespace

// A failed materialization must leave the entry borrowing: the very next
// key() succeeds and returns the original bytes, and the document stays whole.
TEST(ExceptionSafety, FailedKeyMaterializationLeavesObjectConsistent) {
	std::string err;
	Json doc = Json::ParseJson(sourceWithLongKey(), err);
	ASSERT_FALSE(doc.isError()) << err;
	const Json& cdoc = doc;  // const iteration -> JsonConstEntry::key() -> strRef()

	std::string before = cdoc.toString();

	auto it = cdoc.begin();
	ASSERT_TRUE(it != cdoc.end());
	const std::string expectedKey = longKey();

	g_allocFailCountdown = 1;  // the very next allocation fails
	EXPECT_THROW(it->key(), std::bad_alloc);
	ASSERT_EQ(g_allocFailCountdown, -1) << "counter did not self-disarm";

	// The object survived: same key comes back, intact and retryable.
	EXPECT_EQ(it->key(), expectedKey);
	// Second retry is stable too (the first retry materialized for real).
	EXPECT_EQ(it->key(), expectedKey);
	// The rest of the document is untouched and serializes identically.
	EXPECT_EQ(cdoc.toString(), before);
}

// Two consecutive failures must not corrupt anything either: after each
// failed attempt the object is still borrowing, and the final successful
// attempt yields the original content.
TEST(ExceptionSafety, RepeatedFailedMaterializationsThenSuccess) {
	std::string err;
	Json doc = Json::ParseJson(sourceWithLongKey(), err);
	ASSERT_FALSE(doc.isError()) << err;
	const Json& cdoc = doc;

	auto it = cdoc.begin();
	ASSERT_TRUE(it != cdoc.end());
	const std::string expectedKey = longKey();

	for (int attempt = 0; attempt < 3; ++attempt) {
		g_allocFailCountdown = 1;
		EXPECT_THROW(it->key(), std::bad_alloc);
	}
	EXPECT_EQ(it->key(), expectedKey);
}

// The failure must be confined to the one key: the other members keep working
// and materializing their keys succeeds normally afterwards.
TEST(ExceptionSafety, FailureDoesNotPoisonOtherEntries) {
	std::string err;
	Json doc = Json::ParseJson(sourceWithLongKey(), err);
	ASSERT_FALSE(doc.isError()) << err;
	const Json& cdoc = doc;

	auto it = cdoc.begin();
	ASSERT_TRUE(it != cdoc.end());
	++it;  // -> "other"
	ASSERT_TRUE(it != cdoc.end());

	g_allocFailCountdown = 1;
	EXPECT_THROW((void)cdoc.begin()->key(), std::bad_alloc);

	// "other" was never involved in the failure and materializes fine.
	EXPECT_EQ(it->key(), "other");
	// toString() on a String node returns the raw, unquoted value.
	EXPECT_EQ(it->value().toString(), "payload");
	// And the failed long key still materializes correctly afterwards.
	EXPECT_EQ(cdoc.begin()->key(), longKey());
	EXPECT_EQ(cdoc.toString(), Json::ParseJson(sourceWithLongKey(), err).toString());
}
