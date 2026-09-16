// Reproducer for the two const-read data races fixed on 2026-09-16 (see
// docs/线程安全审查与修复-2026-09-16.md).  mode 0: lazy buildKeymap(); mode 1:
// StoredString::strRef() materializing a borrowed key.
//
// Run it against the PARENT of the fixing commit to see the two ASan reports and
// against the current header to see both modes clean; the fix is only evidenced by
// the pair, never by the clean run alone.
//
// mode 1 matters twice: key() no longer materializes anything (it returns a
// string_view), so names past the parser's inline threshold borrow from the arena
// again - and a borrowed name is exactly the storage the old code had to rewrite in
// place.  Keep the probe's key longer than the inline threshold so this stays true.
#include "zjson.hpp"

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <thread>
#include <vector>

using namespace ZJSON;

static std::atomic<int> g_generation{0};
static std::atomic<int> g_arrived{0};
static std::atomic<Json*> g_document{nullptr};
static int g_rounds = 200;
static int g_threads = 8;
static int g_keys = 4000;

static void awaitGeneration(int target) {
	while (g_generation.load(std::memory_order_acquire) < target)
		std::this_thread::yield();
}

int main(int argc, char** argv) {
	const int mode = (argc > 1) ? std::atoi(argv[1]) : 0;
	if (argc > 2) g_rounds = std::atoi(argv[2]);
	if (argc > 3) g_threads = std::atoi(argv[3]);
	if (argc > 4) g_keys = std::atoi(argv[4]);

	printf("mode=%d rounds=%d threads=%d keys=%d\n", mode, g_rounds, g_threads, g_keys);
	fflush(stdout);

	std::vector<std::thread> workers;
	workers.reserve(static_cast<size_t>(g_threads));
	for (int t = 0; t < g_threads; ++t) {
		workers.emplace_back([mode]() {
			for (int round = 0; round < g_rounds; ++round) {
				awaitGeneration(round + 1);
				Json* document = g_document.load(std::memory_order_acquire);
				if (document) {
					if (mode == 0) {
						// First lookup on the fresh document => both/all threads
						// enter buildKeymap() and race on `keymap`.
						volatile bool found = document->contains("k5");
						(void)found;
					} else {
						// First key() on the fresh document => all threads walk the
						// borrowed name that used to be rewritten in place.
						Json::const_iterator it = document->cbegin();
						const std::string_view key = (*it).key();
						volatile size_t length = key.size();
						(void)length;
					}
				}
				g_arrived.fetch_add(1, std::memory_order_acq_rel);
			}
		});
	}

	for (int round = 0; round < g_rounds; ++round) {
		Json fresh;
		if (mode == 0) {
			fresh = Json(JsonType::Object);
			for (int i = 0; i < g_keys; ++i)
				fresh.add("k" + std::to_string(i), i);
		} else {
			// Exactly one borrowed string (the key) => the arena's control block
			// refcount is 1, so a duplicated decrement frees it immediately.
			// The key is longer than the parser's inline-name threshold on purpose,
			// so the name is a VIEW into the arena - the storage that a materializing
			// key() would have had to rewrite.
			std::string err;
			fresh = Json::ParseJson(
				"{\"member_key_0_that_is_definitely_longer_than_the_inline_threshold\":1}", err);
			if (fresh.isError()) {
				printf("parse failed: %s\n", err.c_str());
				return 2;
			}
		}

		g_document.store(&fresh, std::memory_order_release);
		g_arrived.store(0, std::memory_order_release);
		g_generation.store(round + 1, std::memory_order_release);
		while (g_arrived.load(std::memory_order_acquire) < g_threads)
			std::this_thread::yield();
		g_document.store(nullptr, std::memory_order_release);
	}

	for (std::thread& worker : workers)
		worker.join();

	printf("mode %d completed %d rounds without a sanitizer report\n", mode, g_rounds);
	return 0;
}
