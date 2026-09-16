// =============================================================================
//  ZJSON :: thread-safety contract suite
//
//  1. Node allocation/deallocation may cross threads (also covered by
//     test_api_coverage.cpp).
//  2. A document that is only *read* may be shared by any number of threads.
//     Regressions here are the reason this file exists: the key index used to be
//     built lazily by const readers, and key() used to rewrite a borrowed name
//     in place - both were data races under AddressSanitizer.  key() now returns
//     a string_view over the node, so no const read path writes to it at all.
//  3. A concurrently *written* document is NOT safe, like std::string.
//
//  Reader tests share ONE freshly parsed document per round and release the
//  readers together: the interesting window is the first lookup on an
//  un-indexed object.
// =============================================================================
#include "gtest/gtest.h"
#include "../src/zjson.hpp"

#include <atomic>
#include <string>
#include <thread>
#include <vector>

using namespace ZJSON;

namespace {

// A document with a wide object (forces the hash key index), an array, an
// escaped key and an escaped value (forces the materializing decode path).
std::string wideDocumentText(int keys) {
	std::string text = "{\"escaped\\u0041key\":\"line1\\nline2\",\"plain\":\"value\",\"list\":[";
	for (int i = 0; i < 8; ++i) {
		if (i)
			text += ',';
		text += std::to_string(i);
	}
	text += "],";
	for (int i = 0; i < keys; ++i) {
		if (i)
			text += ',';
		text += "\"k" + std::to_string(i) + "\":" + std::to_string(i);
	}
	text += '}';
	return text;
}

// A key longer than the parser's inline-name threshold, so its StoredString is a
// VIEW into the source arena - the storage that a materializing key() would have
// had to rewrite in place.
const char* const borrowedMemberKey = "a_member_key_that_is_definitely_longer_than_the_inline_limit";

// One inline-owned member and one arena-borrowed member, both values numbers.
// The borrowed name is then the document's ONLY borrow, i.e. the arena's reference
// count is exactly 1 - which is what made two concurrent materializations free it.
const std::string& keyStorageDocumentText() {
	static const std::string text =
		std::string("{\"shortk\":1,\"") + borrowedMemberKey + "\":2}";
	return text;
}

// Every const read path that the library documents as read-only.
void exerciseConstReads(const Json& document, int keys, std::atomic<int>& failures) {
	if (!document.contains("k5"))
		failures.fetch_add(1);
	if (document["k5"].toInt() != 5)
		failures.fetch_add(1);
	const std::string lastKey = "k" + std::to_string(keys - 1);
	const Json* found = document.findPtr(lastKey);
	if (!found || found->toInt() != keys - 1)
		failures.fetch_add(1);
	if (document.atRef("/k7").toInt() != 7)
		failures.fetch_add(1);
	if (document.at("/list/3").toInt() != 3)
		failures.fetch_add(1);
	if (!document["missing"].isError())
		failures.fetch_add(1);

	// Escaped key + escaped value: the arena-materialized decode path.
	if (document["escapedAkey"].toString() != "line1\nline2")
		failures.fetch_add(1);

	// Iteration must expose key text without rewriting the node (key() returns a
	// view, so long keys borrow from the arena and short ones live inline).
	size_t members = 0;
	for (Json::const_iterator it = document.cbegin(); it != document.cend(); ++it) {
		const std::string_view key = (*it).key();
		if (key.empty())
			failures.fetch_add(1);
		++members;
	}
	if (members != static_cast<size_t>(keys) + 3)
		failures.fetch_add(1);

	// Whole-tree reads: size estimate, serialization, deep comparison.
	const std::string compact = document.toString();
	if (compact.find("\"" + lastKey + "\"") == std::string::npos)
		failures.fetch_add(1);
	const Json copy = document;
	if (!(copy == document))
		failures.fetch_add(1);
	if (document.toVector().size() != 0)          // object, not array
		failures.fetch_add(1);
}

} // namespace

// -----------------------------------------------------------------------------
// Contract 2: N threads run the whole const read battery on one shared
// document, fresh per round so the lazy key-index build races every time.
// -----------------------------------------------------------------------------
TEST(TestThreadSafety, concurrent_const_reads_of_one_shared_document_are_safe) {
	const int threadCount = 8;
	const int rounds = 30;
	const int keys = 2000;
	const std::string text = wideDocumentText(keys);

	std::atomic<int> failures{ 0 };
	std::atomic<int> generation{ 0 };
	std::atomic<int> arrived{ 0 };
	std::atomic<const Json*> shared{ nullptr };

	std::vector<std::thread> readers;
	readers.reserve(static_cast<size_t>(threadCount));
	for (int t = 0; t < threadCount; ++t) {
		readers.emplace_back([&]() {
			for (int round = 0; round < rounds; ++round) {
				while (generation.load(std::memory_order_acquire) < round + 1)
					std::this_thread::yield();
				const Json* document = shared.load(std::memory_order_acquire);
				if (document)
					exerciseConstReads(*document, keys, failures);
				arrived.fetch_add(1, std::memory_order_acq_rel);
			}
		});
	}

	for (int round = 0; round < rounds; ++round) {
		std::string err;
		const Json document = Json::ParseJson(text, err);
		ASSERT_FALSE(document.isError()) << err;

		shared.store(&document, std::memory_order_release);
		arrived.store(0, std::memory_order_release);
		generation.store(round + 1, std::memory_order_release);
		while (arrived.load(std::memory_order_acquire) < threadCount)
			std::this_thread::yield();
		shared.store(nullptr, std::memory_order_release);
	}

	for (std::thread& reader : readers)
		reader.join();

	EXPECT_EQ(failures.load(), 0);
}

// -----------------------------------------------------------------------------
// The key() path that used to rewrite the node in place, exercised on BOTH storage
// branches: `shortk` is owned inline, `borrowedMemberKey` borrows from the arena.
// -----------------------------------------------------------------------------
TEST(TestThreadSafety, concurrent_key_reads_never_rewrite_the_node) {
	const int threadCount = 8;
	const int rounds = 400;
	const std::string expected =
		std::string("shortk") + borrowedMemberKey;
	std::atomic<int> failures{ 0 };
	std::atomic<int> generation{ 0 };
	std::atomic<int> arrived{ 0 };
	std::atomic<const Json*> shared{ nullptr };

	std::vector<std::thread> readers;
	readers.reserve(static_cast<size_t>(threadCount));
	for (int t = 0; t < threadCount; ++t) {
		readers.emplace_back([&]() {
			for (int round = 0; round < rounds; ++round) {
				while (generation.load(std::memory_order_acquire) < round + 1)
					std::this_thread::yield();
				const Json* document = shared.load(std::memory_order_acquire);
				if (document) {
					std::string joined;
					size_t seen = 0;
					for (Json::const_iterator it = document->cbegin(); it != document->cend(); ++it) {
						const std::string_view key = (*it).key();
						joined.append(key);   // string_view -> append is C++17
						++seen;
					}
					if (seen != 2 || joined != expected)
						failures.fetch_add(1);
				}
				arrived.fetch_add(1, std::memory_order_acq_rel);
			}
		});
	}

	for (int round = 0; round < rounds; ++round) {
		std::string err;
		const Json document = Json::ParseJson(keyStorageDocumentText(), err);
		ASSERT_FALSE(document.isError()) << err;

		shared.store(&document, std::memory_order_release);
		arrived.store(0, std::memory_order_release);
		generation.store(round + 1, std::memory_order_release);
		while (arrived.load(std::memory_order_acquire) < threadCount)
			std::this_thread::yield();
		shared.store(nullptr, std::memory_order_release);
	}

	for (std::thread& reader : readers)
		reader.join();

	EXPECT_EQ(failures.load(), 0);
}

// -----------------------------------------------------------------------------
// Contract 1: allocation/deallocation may cross threads, including a thread
// that has already exited.
// -----------------------------------------------------------------------------
TEST(TestThreadSafety, nodes_may_be_allocated_and_released_on_different_threads) {
	std::atomic<bool> built{ false };
	std::atomic<int> failures{ 0 };
	Json document;

	std::thread producer([&]() {
		Json local(JsonType::Object);
		for (int i = 0; i < 500; ++i)
			local.add("k" + std::to_string(i), Json::array({ i, "v" + std::to_string(i), i % 2 == 0 }));
		document = std::move(local);
		built.store(true, std::memory_order_release);
	});
	producer.join();                       // the allocating thread is now gone

	ASSERT_TRUE(built.load(std::memory_order_acquire));
	EXPECT_EQ(document["k499"][0].toInt(), 499);

	// Mutate, read and finally release nodes whose blocks came from the exited
	// thread's pool.
	std::thread consumer([&]() {
		document.add("extra", 1);
		if (!document.contains("extra"))
			failures.fetch_add(1);
	});
	consumer.join();

	EXPECT_EQ(failures.load(), 0);
	EXPECT_FALSE(document.toString().empty());
}

// -----------------------------------------------------------------------------
// Contract 1 + 3: independent documents used from independent threads must not
// interfere.
// -----------------------------------------------------------------------------
TEST(TestThreadSafety, independent_documents_are_usable_in_parallel) {
	const int threadCount = 4;
	const int rounds = 200;
	const std::string text = wideDocumentText(200);

	std::atomic<int> failures{ 0 };
	std::vector<std::thread> workers;
	workers.reserve(static_cast<size_t>(threadCount));
	for (int t = 0; t < threadCount; ++t) {
		workers.emplace_back([&, t]() {
			for (int round = 0; round < rounds; ++round) {
				std::string err;
				Json document = Json::ParseJson(text, err);
				if (document.isError()) {
					failures.fetch_add(1);
					continue;
				}
				document.add("thread", t);
				if (document["thread"].toInt() != t)
					failures.fetch_add(1);
				if (document["k199"].toInt() != 199)
					failures.fetch_add(1);
				document.remove("k0");
				if (document.contains("k0"))
					failures.fetch_add(1);
				const std::string serialized = document.toString();
				if (serialized.find("\"thread\"") == std::string::npos)
					failures.fetch_add(1);
			}
		});
	}
	for (std::thread& worker : workers)
		worker.join();

	EXPECT_EQ(failures.load(), 0);
}
