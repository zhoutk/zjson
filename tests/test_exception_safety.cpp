// =============================================================================
//  ZJSON :: exception-safety suite (fault injection via global operator new)
//
//  What is locked down here changed on 2026-09-16.  Object member names used to
//  be borrowed views that JsonEntry::key() materialized lazily - the one const
//  read that could throw, and the one that made concurrent const reads a data
//  race.  Names are owned at parse time now, so key() allocates nothing and
//  cannot throw.  These tests pin that contract: arming a fail-on-Nth-allocation
//  counter and calling key() must leave the counter armed.
//
//  The injection replaces the global operator new/delete family with
//  malloc/free plus a countdown, which is only valid because this translation
//  unit is its own test binary.
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

// Longer than any SSO buffer, so materializing it into a std::string would be
// exactly one allocation - i.e. any regression in key() shows up immediately.
std::string longKey() {
	return std::string(80, 'K');
}

std::string sourceWithLongKey() {
	return "{\"" + longKey() + "\":1,\"other\":\"payload\"}";
}

}  // namespace

// key() is a pure read: no allocation, therefore no throw, therefore no race.
TEST(ExceptionSafety, KeyReadOnParsedDocumentAllocatesNothing) {
	std::string err;
	Json doc = Json::ParseJson(sourceWithLongKey(), err);
	ASSERT_FALSE(doc.isError()) << err;
	const Json& cdoc = doc;

	auto it = cdoc.begin();
	ASSERT_TRUE(it != cdoc.end());
	const std::string expectedKey = longKey();

	g_allocFailCountdown = 1;
	EXPECT_NO_THROW(it->key());
	EXPECT_EQ(g_allocFailCountdown, 1) << "key() allocated - it is no longer a pure read";
	EXPECT_EQ(it->key(), expectedKey);

	// Harness is live: the next real allocation does fail.
	EXPECT_THROW(std::string(80, 'x'), std::bad_alloc);
	ASSERT_EQ(g_allocFailCountdown, -1);
	// And the document is untouched by all of the above.
	EXPECT_EQ(it->key(), expectedKey);
}

// Repeated reads, several keys, counter armed every time.
TEST(ExceptionSafety, RepeatedKeyReadsStayAllocationFree) {
	std::string err;
	Json doc = Json::ParseJson(sourceWithLongKey(), err);
	ASSERT_FALSE(doc.isError()) << err;
	const Json& cdoc = doc;
	const std::string expectedKey = longKey();   // computed before arming the counter
	std::string before = cdoc.toString();

	for (int attempt = 0; attempt < 3; ++attempt) {
		g_allocFailCountdown = 1;
		auto it = cdoc.begin();
		EXPECT_NO_THROW(it->key());
		EXPECT_EQ(g_allocFailCountdown, 1);
		EXPECT_EQ(it->key(), expectedKey);
	}

	// Counter disarmed: the whole document still reads and serializes the same.
	g_allocFailCountdown = -1;
	EXPECT_EQ(cdoc.begin()->key(), expectedKey);
	EXPECT_EQ(cdoc.toString(), before);
}

// A deep copy reads the same way - the copy owns its names too.
TEST(ExceptionSafety, CopiedDocumentKeysAreAlsoPureReads) {
	std::string err;
	Json doc = Json::ParseJson(sourceWithLongKey(), err);
	ASSERT_FALSE(doc.isError()) << err;
	Json copy = doc;
	const Json& ccopy = copy;

	auto it = ccopy.begin();
	ASSERT_TRUE(it != ccopy.end());
	const std::string expectedKey = longKey();

	g_allocFailCountdown = 1;
	EXPECT_NO_THROW(it->key());
	EXPECT_EQ(g_allocFailCountdown, 1);
	EXPECT_EQ(it->key(), expectedKey);
	EXPECT_EQ((++it)->key(), "other");
	g_allocFailCountdown = -1;   // disarm before the test tears down
}
