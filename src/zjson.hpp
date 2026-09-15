#pragma once
#pragma execution_character_set("utf-8")

#include <string>
#include <vector>
#include <iostream>
#include <algorithm>
#include <array>
#include <cstring>
#include <cmath>
#include <fstream>
#include <cstdio>
#include <iterator>
#include <unordered_map>
#include <charconv>
#include <system_error>
#include <tuple>
#include <type_traits>
#include <memory>
#include <deque>
#include <string_view>
#include <cstddef>
#include <cstdint>
#include <cctype>
#include <climits>
#include <cstdlib>
#include <limits>
#include <new>
#include <optional>
#include <utility>

// ---------------------------------------------------------------------------
// LeakSanitizer support for the node pool.
//
// The pool deliberately keeps its slabs for the whole process lifetime (see
// jsonNodeAllocator below), so LeakSanitizer has to be told that this memory is
// expected to stay resident instead of reporting it as a leak. LeakSanitizer
// does not exist on Windows even when AddressSanitizer is enabled, and calling
// into it without the runtime linked would fail at link time, so the exemption
// is compiled only where it is actually available.
// ---------------------------------------------------------------------------
#if defined(__SANITIZE_ADDRESS__)
#  define ZJSON_ASAN_DETECTED 1
#elif defined(__has_feature)
#  if __has_feature(address_sanitizer)
#    define ZJSON_ASAN_DETECTED 1
#  endif
#endif

#if defined(ZJSON_ASAN_DETECTED) && !defined(_WIN32) && defined(__has_include)
#  if __has_include(<sanitizer/lsan_interface.h>)
#    include <sanitizer/lsan_interface.h>
#    define ZJSON_LSAN_EXEMPTION 1
#  endif
#endif
#undef ZJSON_ASAN_DETECTED

namespace ZJSON {
	using std::string;
	using std::string_view;
	using std::move;
	using std::vector;

	inline constexpr int max_depth = 100;
	inline constexpr const char* TYPENAMES[8] = { "Error", "False", "True", "Null", "Number", "String", "Object", "Array" };

	enum class JsonType
	{
		Object = 6,
		Array = 7
	};

	class Json;

	// ----------------------------------------------------------------------
	// Number → string serialization (RFC 8259 compliant).
	// Uses std::to_chars (C++17, shortest round-trip) when the implementation
	// provides the floating-point overload; otherwise falls back to a
	// locale-independent snprintf with %.17g (which is round-trip safe for
	// IEEE-754 double precision).
	// ----------------------------------------------------------------------
	namespace detail {
		// ----------------------------------------------------------------------
		// Numeric state of a Number node (R5-1 three-state Number).
		//
		// The model used to be "double only", which silently rounded every integer
		// literal beyond 2^53: `{"id":9007199254740993}` came back as `...992`.  A
		// Number node now carries a one-byte kind next to `type` - inside what used
		// to be alignment padding, so the node does not grow - and the union below
		// reuses exactly the 8 bytes that held the double.
		//
		// Selection rule (see JsonParser::parse_number and the arithmetic
		// constructors): an integer literal that fits int64 is Int64, one that only
		// fits uint64 is Uint64, and everything else - fractional or exponential
		// forms, and literals too wide for both - stays Double, which is exactly the
		// historical behaviour.
		// ----------------------------------------------------------------------
		enum class NumberKind : unsigned char {
			Double = 0,
			Int64 = 1,
			Uint64 = 2
		};

		// Payload of a Number node: 8 bytes whatever the kind.  Trivially copyable
		// and default-initialised to zero, so a node that never becomes a Number is
		// still fully initialised.
		union NumberData {
			double asDouble;
			int64_t asInt64;
			uint64_t asUint64;

			NumberData() : asUint64(0) {}
			NumberData(double value) : asDouble(value) {}
			NumberData(int64_t value) : asInt64(value) {}
			NumberData(uint64_t value) : asUint64(value) {}
		};

		// Text that a parsed document refers to.  `source` owns the raw input (whether
		// copied or moved in) and `materialized` holds the strings that had to be
		// decoded (escapes) - those live in chunked bump storage, so materialising N
		// strings costs one allocation per 4 KB chunk instead of one per string.
		struct StringArena {
			explicit StringArena(const string& text) : source(std::make_shared<string>(text)) {}
			explicit StringArena(string&& text) : source(std::make_shared<string>(std::move(text))) {}

			static constexpr size_t chunkSize = 4096;

			std::shared_ptr<string> source;
			std::vector<std::unique_ptr<char[]>> chunks;
			std::vector<size_t> chunkCapacity;
			size_t used = 0;

			string_view view(size_t offset, size_t length) const {
				return string_view(source->data() + offset, length);
			}

			string_view store(string&& value) {
				const size_t need = value.size();
				if (need == 0)
					return string_view();
				if (chunks.empty() || used + need > chunkCapacity.back()) {
					const size_t bytes = need > chunkSize ? need : chunkSize;
					chunks.push_back(std::unique_ptr<char[]>(new char[bytes]));
					chunkCapacity.push_back(bytes);
					used = 0;
				}
				char* destination = chunks.back().get() + used;
				std::memcpy(destination, value.data(), need);
				used += need;
				return string_view(destination, need);
			}
		};

		class StoredString {
			mutable string owned;
			mutable string_view ref;
			mutable std::shared_ptr<StringArena> arena;
			mutable bool usingRef;

		public:
			StoredString() : ref(), usingRef(false) {}
			StoredString(const char* value) : owned(value ? value : ""), ref(), usingRef(false) {}
			StoredString(const string& value) : owned(value), ref(), usingRef(false) {}
			StoredString(string&& value) : owned(std::move(value)), ref(), usingRef(false) {}

			static StoredString fromView(std::shared_ptr<StringArena> owner, string_view value) {
				StoredString s;
				s.arena = std::move(owner);
				s.ref = value;
				s.usingRef = true;
				return s;
			}

			StoredString(const StoredString&) = default;
			StoredString(StoredString&&) noexcept = default;
			StoredString& operator=(const StoredString&) = default;
			StoredString& operator=(StoredString&&) noexcept = default;

			StoredString& operator=(const char* value) {
				owned = value ? value : "";
				ref = string_view();
				arena.reset();
				usingRef = false;
				return *this;
			}

			StoredString& operator=(const string& value) {
				owned = value;
				ref = string_view();
				arena.reset();
				usingRef = false;
				return *this;
			}

			StoredString& operator=(string&& value) {
				owned = std::move(value);
				ref = string_view();
				arena.reset();
				usingRef = false;
				return *this;
			}

			string_view view() const {
				return usingRef ? ref : string_view(owned.data(), owned.size());
			}

			string str() const {
				string_view v = view();
				if (v.empty())
					return string();
				return string(v.data(), v.size());
			}

			const string& strRef() const {
				if (usingRef) {
					if (ref.empty())
						owned.clear();
					else
						owned.assign(ref.data(), ref.size());
					usingRef = false;
					ref = string_view();
					arena.reset();
				}
				return owned;
			}

			bool empty() const { return view().empty(); }
			size_t size() const { return view().size(); }
			size_t length() const { return view().size(); }
			void clear() {
				owned.clear();
				ref = string_view();
				arena.reset();
				usingRef = false;
			}

			friend bool operator==(const StoredString& lhs, const StoredString& rhs) { return lhs.view() == rhs.view(); }
			friend bool operator!=(const StoredString& lhs, const StoredString& rhs) { return !(lhs == rhs); }
			friend bool operator==(const StoredString& lhs, string_view rhs) { return lhs.view() == rhs; }
			friend bool operator!=(const StoredString& lhs, string_view rhs) { return !(lhs == rhs); }
			friend bool operator==(string_view lhs, const StoredString& rhs) { return lhs == rhs.view(); }
			friend bool operator!=(string_view lhs, const StoredString& rhs) { return !(lhs == rhs); }
			friend bool operator==(const StoredString& lhs, const string& rhs) { return lhs.view() == string_view(rhs.data(), rhs.size()); }
			friend bool operator!=(const StoredString& lhs, const string& rhs) { return !(lhs == rhs); }
			friend bool operator==(const string& lhs, const StoredString& rhs) { return string_view(lhs.data(), lhs.size()) == rhs.view(); }
			friend bool operator!=(const string& lhs, const StoredString& rhs) { return !(lhs == rhs); }
			operator string_view() const { return view(); }
			operator string() const { return str(); }
		};

		// Flat open-addressing index from an object member's key to its node.
		//
		// This replaces the two std::unordered_map instances the library used to keep:
		// the parser's per-object duplicate-key index and the lazy per-object "keymap"
		// behind operator[]/contains/findPtr.  Measured on clang -O2 (2026-09-15):
		//   * inserting N keys into unordered_map<string_view, Json*> cost 45 ns/key at
		//     N=300 and 77 ns/key at N=3000 - one heap node per key plus rehashes.  That
		//     was 42% of the whole parse of a 300-key flat document, and it is why a flat
		//     document parsed at 102-131 ns/node against 47-52 ns/node for nested ones.
		//   * unordered_map<string, Json*> additionally copied every key into a string.
		// A flat table does the same job 2.6-10x faster with a single allocation that
		// doubles, so the per-key allocation disappears entirely.
		//
		// The key text is read back from node->name on every comparison instead of being
		// stored, so no string_view can dangle when a member is renamed and no key is
		// ever copied.  That also keeps the semantics of the map it replaces: the last
		// indexer wins for a duplicate key (plain assignment on the existing slot).
		//
		// Invariants: `capacity` is 0 (empty) or a power of two, and count <= capacity/2,
		// so a linear probe always reaches an empty slot and terminates.
		template <typename Node>
		struct JsonKeyIndex {
			struct Slot {
				size_t hash;
				Node* node;
			};

			std::unique_ptr<Slot[]> slots;
			size_t capacity = 0;
			size_t count = 0;

			// FNV-1a over the key, finished with a mix so the low bits used as the slot
			// index are as well distributed as the high ones.
			static size_t hashKey(string_view key) noexcept {
				size_t h = 1469598103934665603ULL;
				for (char ch : key) {
					h ^= static_cast<unsigned char>(ch);
					h *= 1099511628211ULL;
				}
				h ^= h >> 33;
				h *= 0xff51afd7ed558ccdULL;
				h ^= h >> 33;
				return h;
			}

			size_t size() const noexcept { return count; }

			// Returns the slot holding `key`, or nullptr when the table has no such key.
			Slot* probe(string_view key, size_t hash) const noexcept {
				if (capacity == 0)
					return nullptr;
				const size_t mask = capacity - 1;
				size_t position = hash & mask;
				while (slots[position].node) {
					if (slots[position].hash == hash && slots[position].node->name.view() == key)
						return &slots[position];
					position = (position + 1) & mask;
				}
				return nullptr;
			}

			// Places a key known to be absent.  Callers must have probed first, and must
			// have grown when the load factor would exceed 1/2.
			void insertNew(size_t hash, Node* node) noexcept {
				const size_t mask = capacity - 1;
				size_t position = hash & mask;
				while (slots[position].node)
					position = (position + 1) & mask;
				slots[position].hash = hash;
				slots[position].node = node;
				++count;
			}

			void allocate(size_t wantedCapacity) {
				std::unique_ptr<Slot[]> fresh(new Slot[wantedCapacity]);
				for (size_t index = 0; index < wantedCapacity; ++index)
					fresh[index].node = nullptr;
				slots = std::move(fresh);
				capacity = wantedCapacity;
				count = 0;
			}

			// Makes room for `wanted` entries and re-places the live ones.  Never call
			// while a Slot* from probe() is still in use: the array is replaced.
			void reserve(size_t wanted) {
				size_t wantedCapacity = 8;
				while (wantedCapacity < wanted * 2)
					wantedCapacity <<= 1;
				if (capacity == wantedCapacity)
					return;
				std::unique_ptr<Slot[]> old = std::move(slots);
				const size_t oldCapacity = capacity;
				allocate(wantedCapacity);
				for (size_t index = 0; index < oldCapacity; ++index)
					if (old[index].node)
						insertNew(old[index].hash, old[index].node);
			}

			void grow() {
				std::unique_ptr<Slot[]> old = std::move(slots);
				const size_t oldCapacity = capacity;
				allocate(capacity == 0 ? 8 : capacity * 2);
				for (size_t index = 0; index < oldCapacity; ++index)
					if (old[index].node)
						insertNew(old[index].hash, old[index].node);
			}

			// Adds `node` under `key`; a key that is already present is replaced, which is
			// what the unordered_map operator[] this replaces did.
			void assign(string_view key, Node* node) {
				const size_t hash = hashKey(key);
				if (Slot* existing = probe(key, hash)) {
					existing->node = node;
					return;
				}
				if (capacity == 0 || (count + 1) * 2 > capacity)
					grow();
				insertNew(hash, node);
			}

			Node* find(string_view key) const noexcept {
				Slot* slot = probe(key, hashKey(key));
				return slot ? slot->node : nullptr;
			}
		};

		// The node pool keeps its slabs for the whole process lifetime on purpose, so
		// LeakSanitizer is explicitly told about them (no-op wherever LSAN is absent).
		inline void markPoolMemoryResident(void* block) noexcept {
#ifdef ZJSON_LSAN_EXEMPTION
			__lsan_ignore_object(block);
#else
			(void)block;
#endif
		}
#undef ZJSON_LSAN_EXEMPTION

		// Per-thread slab pool for Json nodes.
		//
		// BlockSize is the node size, so the "slab block or oversized block" decision is
		// a compile-time comparison that holds on every thread - including a thread that
		// frees a node allocated elsewhere without ever having allocated one itself.
		template <size_t BlockSize>
		class SlabAllocator {
			struct FreeNode { FreeNode* next; };
			static constexpr size_t blocksPerSlab = 1024;

			static constexpr size_t alignUp(size_t value) {
				const size_t alignment = alignof(std::max_align_t);
				return ((value + alignment - 1) / alignment) * alignment;
			}

			static constexpr size_t blockSize = alignUp(BlockSize < sizeof(FreeNode) ? sizeof(FreeNode) : BlockSize);

			FreeNode* freeList = nullptr;
			std::vector<void*> slabs;

			// The pool belongs to one thread only, so the free-list helpers below need no
			// locking: they are only ever reached from allocate()/deallocate() of the
			// owning thread. Blocks freed by another thread simply join this thread's
			// free list - they stay valid because no slab is ever released.
			void pushFreeBlock(void* ptr) noexcept {
				auto* node = static_cast<FreeNode*>(ptr);
				node->next = freeList;
				freeList = node;
			}

			void* popFreeBlock() noexcept {
				FreeNode* node = freeList;
				freeList = freeList->next;
				return node;
			}

			void addSlab() {
				const size_t bytes = blockSize * blocksPerSlab;
				void* raw = ::operator new(bytes);
				// Never released on purpose (see jsonNodeAllocator), so it must be reported to
				// LeakSanitizer as intentionally resident.
				markPoolMemoryResident(raw);
				slabs.push_back(raw);
				char* cursor = static_cast<char*>(raw);
				for (size_t i = 0; i < blocksPerSlab; ++i) {
					auto* node = reinterpret_cast<FreeNode*>(cursor + i * blockSize);
					node->next = freeList;
					freeList = node;
				}
			}

		public:
			// No destructor on purpose: slabs live for the whole process so that blocks
			// parked in another thread's free list (or in another module that inlined this
			// header) can never dangle.
			void* allocate(size_t size) {
				if (size != blockSize)
					return ::operator new(size);		// not a node-sized request
				if (!freeList)
					addSlab();
				return popFreeBlock();
			}

			void deallocate(void* ptr, size_t size) noexcept {
				if (!ptr)
					return;
				// Blocks that are not node-sized were handed out directly by ::operator new,
				// so they must be returned the same way. Without this split such a block would
				// be pushed onto the node free list and later reused at the wrong size (heap
				// corruption).
				if (size != blockSize) {
					::operator delete(ptr);
					return;
				}
				pushFreeBlock(ptr);
			}

			// Fallback used only if the compiler selects the unsized deallocation; every
			// block that can reach it is node-sized, so the free list stays consistent.
			//
			// The invariant is structural: allocate() serves requests that are exactly
			// BlockSize and hands anything else to ::operator new, so a pointer arriving
			// here is always a slab block. Keep the two deallocation overloads in step if
			// a second node size is ever introduced.
			void deallocate(void* ptr) noexcept {
				if (!ptr)
					return;
				pushFreeBlock(ptr);
			}
		};

		// NOTE: the node pool is per-thread and deliberately process-lifetime (its slabs
		// are never released).  Json nodes may be freed on a different thread - or inside
		// a different shared library, since this header is inlined into every module -
		// than the one that allocated them.  Because no slab is ever released, those
		// parked blocks always point at live memory, which is what makes a thread_local
		// pool safe here; and because each thread only touches its own pool, no locking
		// is needed at all.  Measured against the alternative (one shared, mutex
		// protected pool): 4-thread parse+stringify scaling x2.3~2.9 here versus
		// x0.42~0.47 there, and a node create+destroy pair costs 25.9~27.6 ns here versus
		// 42.7~45.4 ns there (docs/评审报告与优化实施方案-2026-09-14.md §8.6).
		template <size_t BlockSize>
		inline SlabAllocator<BlockSize>& jsonNodeAllocator() {
			static thread_local SlabAllocator<BlockSize> allocator;
			return allocator;
		}

		template <typename T, typename = void>
		struct has_fp_to_chars : std::false_type {};

		template <typename T, typename = void>
		struct has_adl_to_json : std::false_type {};

		// A LIFO stack that keeps its first InlineCapacity entries inside the object and
		// only reaches for the heap when a document nests deeper than that.  The iterative
		// traversals (clone, destroy, measure, serialize, compare) use it, so ordinary
		// documents - which are shallow - allocate nothing at all, while arbitrarily deep
		// ones still work.
		template <typename T, size_t InlineCapacity>
		class SmallStack {
		public:
			bool empty() const noexcept { return inlineCount_ == 0 && overflow_.empty(); }
			T& back() noexcept { return overflow_.empty() ? inline_[inlineCount_ - 1] : overflow_.back(); }
			void push_back(const T& value) {
				if (overflow_.empty() && inlineCount_ < InlineCapacity) {
					inline_[inlineCount_++] = value;
					return;
				}
				overflow_.push_back(value);
			}
			void pop_back() {
				if (!overflow_.empty()) {
					overflow_.pop_back();
					return;
				}
				--inlineCount_;
			}

		private:
			T inline_[InlineCapacity]{};
			size_t inlineCount_ = 0;
			std::vector<T> overflow_;
		};

		template <typename T>
		struct has_adl_to_json<T, std::void_t<decltype(to_json(std::declval<Json&>(), std::declval<const T&>()))>>
			: std::true_type {};

		template <typename T, typename = void>
		struct has_adl_from_json : std::false_type {};

		template <typename T>
		struct has_adl_from_json<T, std::void_t<decltype(from_json(std::declval<const Json&>(), std::declval<T&>()))>>
			: std::true_type {};

		template <typename T>
		struct has_fp_to_chars<T, std::void_t<decltype(std::to_chars(
			std::declval<char*>(), std::declval<char*>(), std::declval<T>()))>>
			: std::true_type {};

		template <typename T, typename = void>
		struct has_fp_from_chars : std::false_type {};

		template <typename T>
		struct has_fp_from_chars<T, std::void_t<decltype(std::from_chars(
			std::declval<const char*>(), std::declval<const char*>(), std::declval<T&>()))>>
			: std::true_type {};

		// Forward declaration: the wrappers below use it, the definition follows so that
		// the parsing rules stay in one readable block.
		inline bool parseSpecialOrHex(string_view text, double& out);

		// Decimal exponent of the leading significant digit of a numeric literal:
		// "0.5" -> -1, "123" -> 2, "1e-999" -> -999, "0.0001e+5" -> 1.  Returns false
		// for a literal without a significant digit (a pure zero, which never goes out of
		// range).  Only used to decide the SIGN of a magnitude far outside the double
		// range, so the explicit exponent is saturated rather than accumulated exactly -
		// no real literal comes close to the bound.
		inline bool leadingDecimalExponent(string_view text, long long& exponent) {
			size_t cursor = 0;
			if (cursor < text.size() && (text[cursor] == '+' || text[cursor] == '-'))
				++cursor;

			size_t digitsSeen = 0;
			size_t digitsBeforePoint = 0;
			size_t firstSignificant = 0;
			bool sawPoint = false;
			bool found = false;

			size_t at = cursor;
			for (; at < text.size(); ++at) {
				const char ch = text[at];
				if (ch >= '0' && ch <= '9') {
					if (!found && ch != '0') {
						found = true;
						firstSignificant = digitsSeen;
					}
					++digitsSeen;
					if (!sawPoint)
						++digitsBeforePoint;
					continue;
				}
				if (ch == '.' && !sawPoint) {
					sawPoint = true;
					continue;
				}
				break;                                  // 'e'/'E', or the end of the number
			}
			if (!found)
				return false;

			// Place value of the leading significant digit, as a power of ten.
			long long place = static_cast<long long>(digitsBeforePoint) - 1 -
				static_cast<long long>(firstSignificant);

			long long scale = 0;
			if (at < text.size() && (text[at] == 'e' || text[at] == 'E')) {
				size_t k = at + 1;
				bool negative = false;
				if (k < text.size() && (text[k] == '+' || text[k] == '-')) {
					negative = text[k] == '-';
					++k;
				}
				for (; k < text.size() && text[k] >= '0' && text[k] <= '9'; ++k) {
					if (scale < 1000000000LL)
						scale = scale * 10 + (text[k] - '0');
				}
				if (negative)
					scale = -scale;
			}

			exponent = place + scale;
			return true;
		}

		// Result for a literal that std::from_chars reported as out of range.
		//
		// The standard only requires the error code in that case: what from_chars leaves
		// in the `double` is unspecified, and all three standard libraries happen to
		// saturate it to +/-inf (overflow) or +/-0 (underflow).  Rather than depend on
		// that, the direction is read off the literal - a magnitude at or above 1
		// overflowed, one below 1 underflowed.  This path is only reached for magnitudes
		// far outside the double range, so its cost does not matter.
		inline double outOfRangeMagnitude(string_view text) {
			const bool negative = !text.empty() && text[0] == '-';
			long long exponent = 0;
			if (!leadingDecimalExponent(text, exponent) || exponent >= 0)
				return negative ? -HUGE_VAL : HUGE_VAL;
			return negative ? -0.0 : 0.0;
		}

		// Locale-independent double parsing. std::from_chars never consults
		// LC_NUMERIC, unlike std::strtod, so "1.5" is not truncated to 1 in
		// comma-decimal locales. Out-of-range magnitudes keep the historical
		// strtod behaviour (saturate to +/-inf or 0), which also keeps the
		// implementation-defined "huge exponent" inputs accepted - but the
		// saturation is decided here from the literal, not taken from from_chars.
		inline double parseDoubleImpl(string_view text, std::true_type /*has_from_chars*/) {
			double value = 0.0;
			auto res = std::from_chars(text.data(), text.data() + text.size(), value);
			if (res.ec == std::errc{})
				return value;
			if (res.ec == std::errc::result_out_of_range)
				return outOfRangeMagnitude(text);
			return parseSpecialOrHex(text, value) ? value : 0.0;
		}

		inline double parseDoubleImpl(string_view text, std::false_type /*no_from_chars*/) {
			// Older standard libraries: std::strtod is the only option, but the
			// special forms are still handled here so the result does not depend on
			// the C locale for them either.
			double value = 0.0;
			if (parseSpecialOrHex(text, value))
				return value;
			string tmp(text);
			return std::strtod(tmp.c_str(), nullptr);
		}

		inline double parseDouble(string_view text) {
			return parseDoubleImpl(text, has_fp_from_chars<double>{});
		}

		// Recognises the forms that std::from_chars rejects but that the historical
		// std::atof-based code accepted: [+-]inf, [+-]infinity, [+-]nan and
		// hexadecimal floating point (0x1.8p1). Parsing them here keeps the result
		// independent of LC_NUMERIC, which std::atof would consult for the radix
		// point of a hex literal.
		inline bool parseSpecialOrHex(string_view text, double& out) {
			if (text.empty())
				return false;

			bool negative = false;
			size_t index = 0;
			if (text[0] == '+' || text[0] == '-') {
				negative = (text[0] == '-');
				index = 1;
			}
			const string_view rest = text.substr(index);
			if (rest.empty())
				return false;
			auto startsWithIgnoreCase = [](string_view candidate, const char* prefix) {
				const size_t length = std::strlen(prefix);
				if (candidate.size() < length)
					return false;
				for (size_t i = 0; i < length; ++i) {
					if (std::tolower(static_cast<unsigned char>(candidate[i])) != prefix[i])
						return false;
				}
				return true;
			};

			if (startsWithIgnoreCase(rest, "inf")) {           // "inf" and "infinity"
				out = negative ? -HUGE_VAL : HUGE_VAL;
				return true;
			}
			if (startsWithIgnoreCase(rest, "nan")) {
				out = std::nan("");
				if (negative)
					out = -out;
				return true;
			}
			if (rest.size() > 1 && rest[0] == '0' && (rest[1] == 'x' || rest[1] == 'X')) {
				auto hexValue = [](char ch) -> int {
					if (ch >= '0' && ch <= '9') return ch - '0';
					if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
					if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
					return -1;
				};

				size_t cursor = 2;
				double mantissa = 0.0;
				int exponent = 0;
				bool anyDigit = false;
				for (; cursor < rest.size(); ++cursor) {
					const int digit = hexValue(rest[cursor]);
					if (digit < 0)
						break;
					mantissa = mantissa * 16.0 + digit;
					anyDigit = true;
				}
				if (cursor < rest.size() && rest[cursor] == '.') {
					++cursor;
					for (; cursor < rest.size(); ++cursor) {
						const int digit = hexValue(rest[cursor]);
						if (digit < 0)
							break;
						mantissa = mantissa * 16.0 + digit;
						exponent -= 4;
						anyDigit = true;
					}
				}
				if (!anyDigit)
					return false;                               // e.g. "0x"
				if (cursor < rest.size() && (rest[cursor] == 'p' || rest[cursor] == 'P')) {
					++cursor;
					bool negativeExponent = false;
					if (cursor < rest.size() && (rest[cursor] == '+' || rest[cursor] == '-')) {
						negativeExponent = (rest[cursor] == '-');
						++cursor;
					}
					int value = 0;
					bool anyExponentDigit = false;
					for (; cursor < rest.size(); ++cursor) {
						if (!std::isdigit(static_cast<unsigned char>(rest[cursor])))
							break;
						value = value * 10 + (rest[cursor] - '0');
						anyExponentDigit = true;
					}
					if (anyExponentDigit)
						exponent += negativeExponent ? -value : value;
				}
				out = std::ldexp(mantissa, exponent);
				if (negative)
					out = -out;
				return true;
			}
			return false;
		}

		// atof()-compatible "parse a leading number, ignore the rest" used by
		// Json::toDouble() for string values: leading whitespace is skipped, the
		// special/hex forms below are recognised first (std::from_chars would otherwise
		// accept the leading "0" of "0x10" and stop there), and everything else is parsed
		// by std::from_chars, which is locale independent and stops at the first
		// character that cannot continue the number - "3.5abc" still yields 3.5.
		inline double parseLeadingDoubleImpl(string_view text, std::true_type /*has_from_chars*/) {
			double special = 0.0;
			if (parseSpecialOrHex(text, special))
				return special;
			double value = 0.0;
			auto res = std::from_chars(text.data(), text.data() + text.size(), value);
			if (res.ec == std::errc{})
				return value;
			if (res.ec == std::errc::result_out_of_range)
				return outOfRangeMagnitude(text);		// not from_chars' saturation
			return 0.0;
		}

		inline double parseLeadingDoubleImpl(string_view text, std::false_type /*no_from_chars*/) {
			double value = 0.0;
			if (parseSpecialOrHex(text, value))
				return value;
			string tmp(text);
			return std::strtod(tmp.c_str(), nullptr);
		}

		inline double parseLeadingDouble(string_view text) {
			const char* first = text.data();
			const char* last = first + text.size();
			while (first != last && std::isspace(static_cast<unsigned char>(*first)))
				++first;
			return parseLeadingDoubleImpl(string_view(first, static_cast<size_t>(last - first)),
				has_fp_from_chars<double>{});
		}

		template <typename T>
		inline void appendDoubleImpl(T v, string& out, std::true_type /*has_to_chars*/) {
			char buf[64];
			auto res = std::to_chars(buf, buf + sizeof(buf), v);
			if (res.ec == std::errc{}) {
				out.append(buf, static_cast<size_t>(res.ptr - buf));
			} else {
				int n = std::snprintf(buf, sizeof(buf), "%.17g", static_cast<double>(v));
				if (n > 0) out.append(buf, static_cast<size_t>(n));
			}
		}

		template <typename T>
		inline void appendDoubleImpl(T v, string& out, std::false_type /*no_fp_to_chars*/) {
			char buf[64];
			int n = std::snprintf(buf, sizeof(buf), "%.17g", static_cast<double>(v));
			if (n > 0) out.append(buf, static_cast<size_t>(n));
		}
	}

	// Integral overloads (R5-1).  An integer that was stored as int64/uint64 is
	// written straight from the exact value, which is what keeps
	// `9007199254740993` from being rounded through a double on the way out.
	//
	// They are declared before the double overload on purpose: the double version
	// delegates its "exact integer" fast path to the int64 one, and a call to an
	// overload that is not declared yet would resolve right back to itself.
	inline void appendNumber(int64_t v, string& out) {
		char buf[24];
		auto res = std::to_chars(buf, buf + sizeof(buf), v);
		if (res.ec == std::errc{}) {
			out.append(buf, static_cast<size_t>(res.ptr - buf));
			return;
		}
		int n = std::snprintf(buf, sizeof(buf), "%lld", static_cast<long long>(v));
		if (n > 0) out.append(buf, static_cast<size_t>(n));
	}

	inline void appendNumber(uint64_t v, string& out) {
		char buf[24];
		auto res = std::to_chars(buf, buf + sizeof(buf), v);
		if (res.ec == std::errc{}) {
			out.append(buf, static_cast<size_t>(res.ptr - buf));
			return;
		}
		int n = std::snprintf(buf, sizeof(buf), "%llu", static_cast<unsigned long long>(v));
		if (n > 0) out.append(buf, static_cast<size_t>(n));
	}

	// Append a JSON-formatted number. NaN/Inf are written as "null" because
	// RFC 8259 forbids them; callers that wish to reject earlier may do so.
	inline void appendNumber(double v, string& out) {
		if (!std::isfinite(v)) {
			out.append("null");
			return;
		}
		// Integer fast-path: exact integer that fits in long long → write %lld.
		// Preserves a leading '-' for negative zero (e.g. -0.0 → "-0").
		if (v == std::floor(v) && v >= -1e16 && v <= 1e16) {
			long long i = static_cast<long long>(v);
			if (static_cast<double>(i) == v) {
				// Negative zero has no integer spelling of its own, so it keeps a
				// sign the integer state cannot carry (-0.0 → "-0").
				if (i == 0 && std::signbit(v)) {
					out.append("-0");
					return;
				}
				appendNumber(static_cast<int64_t>(i), out);
				return;
			}
		}
		detail::appendDoubleImpl(v, out, detail::has_fp_to_chars<double>{});
	}

	inline void appendControlEscape(unsigned char ch, string& out) {
		static constexpr char hex[] = "0123456789abcdef";
		char escaped[6] = { '\\', 'u', '0', '0', hex[ch >> 4], hex[ch & 0x0F] };
		out.append(escaped, sizeof(escaped));
	}

	inline void appendEscapedString(string_view input, string& out) {
		const char* data = input.data();
		const size_t length = input.size();
		size_t chunkStart = 0;

		for (size_t index = 0; index < length; ++index) {
			const unsigned char ch = static_cast<unsigned char>(data[index]);
			if (ch >= 0x20 && ch != '"' && ch != '\\')
				continue;

			if (index > chunkStart)
				out.append(data + chunkStart, index - chunkStart);

			switch (ch) {
			case '"': out.append("\\\"", 2); break;
			case '\\': out.append("\\\\", 2); break;
			case '\b': out.append("\\b", 2); break;
			case '\f': out.append("\\f", 2); break;
			case '\n': out.append("\\n", 2); break;
			case '\r': out.append("\\r", 2); break;
			case '\t': out.append("\\t", 2); break;
			default:
				appendControlEscape(ch, out);
				break;
			}
			chunkStart = index + 1;
		}

		if (chunkStart < length)
			out.append(data + chunkStart, length - chunkStart);
	}

	inline void appendQuotedKey(string_view key, string& out) {
		out.push_back('"');
		appendEscapedString(key, out);
		out.append("\":", 2);
	}

	// UTF-8 byte sequence validator (RFC 3629)
	inline bool validate_utf8_bytes(const string& s, size_t& errorPos) {
		size_t len = s.size();
		size_t pos = 0;
		while (pos < len) {
			unsigned char b0 = static_cast<unsigned char>(s[pos]);
			if (b0 <= 0x7F) { pos++; continue; }
			int seqLen;
			uint32_t cp;
			if ((b0 & 0xE0) == 0xC0) { seqLen = 2; cp = b0 & 0x1F; }
			else if ((b0 & 0xF0) == 0xE0) { seqLen = 3; cp = b0 & 0x0F; }
			else if ((b0 & 0xF8) == 0xF0) { seqLen = 4; cp = b0 & 0x07; }
			else { errorPos = pos; return false; }
			if (pos + seqLen > len) { errorPos = pos; return false; }
			for (int j = 1; j < seqLen; j++) {
				unsigned char bj = static_cast<unsigned char>(s[pos + j]);
				if ((bj & 0xC0) != 0x80) { errorPos = pos; return false; }
				cp = (cp << 6) | (bj & 0x3F);
			}
			if (seqLen == 2 && cp < 0x80) { errorPos = pos; return false; }
			if (seqLen == 3 && cp < 0x800) { errorPos = pos; return false; }
			if (seqLen == 4 && cp < 0x10000) { errorPos = pos; return false; }
			if (cp >= 0xD800 && cp <= 0xDFFF) { errorPos = pos; return false; }
			if (cp > 0x10FFFF) { errorPos = pos; return false; }
			pos += seqLen;
		}
		return true;
	}

	// ---------------------------------------------------------------------------
	// Output sinks.
	//
	// Serialization is written through one of these, so the very same iterative
	// writer can either fill a std::string (toString) or stream straight to an
	// std::ostream (dumpTo) without materialising the whole document first.
	// ---------------------------------------------------------------------------
	namespace detail {

	// Escaped form of a single byte; returns the number of bytes written (2 or 6).
	inline size_t escapeByte(unsigned char ch, char* out) {
		switch (ch) {
		case '"': out[0] = '\\'; out[1] = '"'; return 2;
		case '\\': out[0] = '\\'; out[1] = '\\'; return 2;
		case '\b': out[0] = '\\'; out[1] = 'b'; return 2;
		case '\f': out[0] = '\\'; out[1] = 'f'; return 2;
		case '\n': out[0] = '\\'; out[1] = 'n'; return 2;
		case '\r': out[0] = '\\'; out[1] = 'r'; return 2;
		case '\t': out[0] = '\\'; out[1] = 't'; return 2;
		default: {
			static constexpr char hex[] = "0123456789abcdef";
			out[0] = '\\'; out[1] = 'u'; out[2] = '0'; out[3] = '0';
			out[4] = hex[ch >> 4];
			out[5] = hex[ch & 0x0F];
			return 6;
		}
		}
	}

	struct StringSink {
		string& out;
		explicit StringSink(string& target) : out(target) {}
		void push_back(char ch) { out.push_back(ch); }
		void append(const char* data, size_t length) { out.append(data, length); }
		void appendNumber(double value) { ZJSON::appendNumber(value, out); }
		void appendNumber(int64_t value) { ZJSON::appendNumber(value, out); }
		void appendNumber(uint64_t value) { ZJSON::appendNumber(value, out); }
		void appendEscaped(string_view text) { appendEscapedString(text, out); }
		void appendIndent(int indentSize, int depth) {
			if (indentSize > 0)
				out.append(static_cast<size_t>(indentSize) * static_cast<size_t>(depth > 0 ? depth : 0), ' ');
		}
	};

	struct StreamSink {
		std::ostream& out;
		explicit StreamSink(std::ostream& target) : out(target) {}
		void push_back(char ch) { out.put(ch); }
		void append(const char* data, size_t length) { out.write(data, static_cast<std::streamsize>(length)); }
		// One reused buffer: numbers are short and this keeps the writer allocation
		// free after the first call.
		void appendNumber(double value) { appendNumberToScratch(value); }
		void appendNumber(int64_t value) { appendNumberToScratch(value); }
		void appendNumber(uint64_t value) { appendNumberToScratch(value); }
		void appendEscaped(string_view text) {
			const char* data = text.data();
			const size_t length = text.size();
			size_t chunkStart = 0;
			for (size_t index = 0; index < length; ++index) {
				const unsigned char ch = static_cast<unsigned char>(data[index]);
				if (ch >= 0x20 && ch != '"' && ch != '\\')
					continue;
				if (index > chunkStart)
					out.write(data + chunkStart, static_cast<std::streamsize>(index - chunkStart));
				char escaped[6];
				const size_t written = escapeByte(ch, escaped);
				out.write(escaped, static_cast<std::streamsize>(written));
				chunkStart = index + 1;
			}
			if (chunkStart < length)
				out.write(data + chunkStart, static_cast<std::streamsize>(length - chunkStart));
		}
		void appendIndent(int indentSize, int depth) {
			if (indentSize > 0)
				out << string(static_cast<size_t>(indentSize) * static_cast<size_t>(depth > 0 ? depth : 0), ' ');
		}

	private:
		template <typename T>
		void appendNumberToScratch(T value) {
			scratch.clear();
			ZJSON::appendNumber(value, scratch);
			out.write(scratch.data(), static_cast<std::streamsize>(scratch.size()));
		}

		string scratch;
	};

	} // namespace detail

	enum class Type {
		Error,
		False,
		True,
		Null,
		Number,
		String,
		Object,
		Array
	};

	class JsonIterator;
	class JsonConstIterator;
	class JsonEntry;
	class JsonConstEntry;

	struct ParseOptions {
		enum class DuplicateKeyPolicy {
			KeepFirst,
			KeepLast,
			Reject
		};

		bool allowComments = true;
		bool validateUtf8 = false;
		DuplicateKeyPolicy duplicateKey = DuplicateKeyPolicy::KeepLast;
	};

	class Json final {
		friend class JsonIterator;
		friend class JsonConstIterator;
		friend class JsonEntry;
		friend class JsonConstEntry;
		// The member index reads node->name to compare keys without storing a copy.
		template <typename Node> friend struct detail::JsonKeyIndex;
	private:
		Json* brother;
		Json* child;
		Json* lastChild;   // tail of child list — O(1) append
		// Lazy O(1) key lookup (Object only).  A flat open-addressing table rather than a
		// map: it compares against the member's own name, so nothing is copied and no
		// string_view can dangle.  See detail::JsonKeyIndex.
		mutable detail::JsonKeyIndex<Json>* keymap;
		Type type;
		// R5-1: which member of `valueNumber` is live.  Declared right after `type`
		// on purpose - this byte lands in the padding that used to follow it, so the
		// third numeric state costs no memory (see detail::NumberData).
		detail::NumberKind numberKind = detail::NumberKind::Double;
		detail::StoredString valueString;
		// Numeric payload.  Never read a union member directly: which one is valid is
		// decided by `numberKind`, so go through numberAs<T>() or one of the
		// setNumber* helpers below.
		detail::NumberData valueNumber;
		detail::StoredString name;

		void setNumberDouble(double value) noexcept {
			this->numberKind = detail::NumberKind::Double;
			this->valueNumber.asDouble = value;
		}
		void setNumberInt64(int64_t value) noexcept {
			this->numberKind = detail::NumberKind::Int64;
			this->valueNumber.asInt64 = value;
		}
		void setNumberUint64(uint64_t value) noexcept {
			this->numberKind = detail::NumberKind::Uint64;
			this->valueNumber.asUint64 = value;
		}

		// The stored value converted to T.  An integer state converts directly
		// (exactly for a T that can hold it) instead of detouring through double,
		// which is what kept 2^53 + 1 from being readable exactly.
		template <typename T>
		T numberAs() const noexcept {
			switch (this->numberKind) {
			case detail::NumberKind::Int64: return static_cast<T>(this->valueNumber.asInt64);
			case detail::NumberKind::Uint64: return static_cast<T>(this->valueNumber.asUint64);
			default: return static_cast<T>(this->valueNumber.asDouble);
			}
		}

		// Writes one Number node through a sink, picking the live payload member.
		template <typename Sink>
		static void writeNumber(const Json* node, Sink& sink) {
			switch (node->numberKind) {
			case detail::NumberKind::Int64: sink.appendNumber(node->valueNumber.asInt64); break;
			case detail::NumberKind::Uint64: sink.appendNumber(node->valueNumber.asUint64); break;
			default: sink.appendNumber(node->valueNumber.asDouble); break;
			}
		}

		static void appendNumberText(const Json* node, string& out) {
			switch (node->numberKind) {
			case detail::NumberKind::Int64: appendNumber(node->valueNumber.asInt64, out); break;
			case detail::NumberKind::Uint64: appendNumber(node->valueNumber.asUint64, out); break;
			default: appendNumber(node->valueNumber.asDouble, out); break;
			}
		}

		// Equality across the three states.  Values that both states can express
		// compare numerically (so Json(1) == Json(1.0) still holds), but a double is
		// only equal to an integer when it is an exact integer in that integer's
		// range - 9007199254740993 must not equal the double it would round to.
		static bool numberEquals(const Json& lhs, const Json& rhs) noexcept {
			const detail::NumberKind lk = lhs.numberKind;
			const detail::NumberKind rk = rhs.numberKind;
			if (lk == rk) {
				switch (lk) {
				case detail::NumberKind::Int64: return lhs.valueNumber.asInt64 == rhs.valueNumber.asInt64;
				case detail::NumberKind::Uint64: return lhs.valueNumber.asUint64 == rhs.valueNumber.asUint64;
				default: return lhs.valueNumber.asDouble == rhs.valueNumber.asDouble;
				}
			}
			if (lk == detail::NumberKind::Int64)
				return numberEqualsIntDouble(lhs.valueNumber.asInt64, rhs);
			if (lk == detail::NumberKind::Uint64)
				return numberEqualsUintDouble(lhs.valueNumber.asUint64, rhs);
			if (rk == detail::NumberKind::Int64)
				return numberEqualsIntDouble(rhs.valueNumber.asInt64, lhs);
			return numberEqualsUintDouble(rhs.valueNumber.asUint64, lhs);
		}

		static bool numberEqualsIntDouble(int64_t value, const Json& other) noexcept {
			if (other.numberKind == detail::NumberKind::Uint64)
				return value >= 0 && static_cast<uint64_t>(value) == other.valueNumber.asUint64;
			const double d = other.valueNumber.asDouble;
			// -2^63 is exactly representable, so this range test has no unsafe edge.
			if (!(d >= -9223372036854775808.0 && d < 9223372036854775808.0))
				return false;                       // also rejects NaN and infinities
			if (d != std::floor(d))
				return false;
			return static_cast<int64_t>(d) == value;
		}

		static bool numberEqualsUintDouble(uint64_t value, const Json& other) noexcept {
			if (other.numberKind == detail::NumberKind::Int64)
				return other.valueNumber.asInt64 >= 0 &&
					static_cast<uint64_t>(other.valueNumber.asInt64) == value;
			const double d = other.valueNumber.asDouble;
			// 2^64 is the first double above the uint64 range; everything below it,
			// and the value 0 itself, is safe to cast.
			if (!(d >= 0.0 && d < 18446744073709551616.0))
				return false;
			if (d != std::floor(d))
				return false;
			return static_cast<uint64_t>(d) == value;
		}

		static inline void appendIndent(string& out, int indentSize, int depth) {
			out.append(static_cast<size_t>(indentSize * depth), ' ');
		}

		size_t childCount() const {
			size_t count = 0;
			for (Json* cur = this->child; cur; cur = cur->brother)
				++count;
			return count;
		}

		// Text comparison used by indexOf(): plain strings compare their raw value,
		// every other kind compares the text toString() would produce. Avoids the
		// temporary std::string that a plain toString() comparison would allocate
		// for each element (the dominant cost for large arrays).
		bool serializedLeafEquals(const Json* node, string_view text) const {
			switch (node->type) {
			case Type::String:
				return node->valueString.view() == text;
			case Type::Number: {
				string tmp;
				appendNumberText(node, tmp);
				return string_view(tmp.data(), tmp.size()) == text;
			}
			case Type::True:
				return text == "true";
			case Type::False:
				return text == "false";
			case Type::Null:
				return text == "null";
			case Type::Error:
				return text.empty();
			default:
				return node->toString() == text;
			}
		}

		Json* directChildByKey(string_view key) {
			for (Json* cur = this->child; cur; cur = cur->brother) {
				if (cur->name == key)
					return cur;
			}
			return nullptr;
		}

		const Json* directChildByKey(string_view key) const {
			for (Json* cur = this->child; cur; cur = cur->brother) {
				if (cur->name == key)
					return cur;
			}
			return nullptr;
		}

		Json* directChildByIndex(size_t index) {
			Json* cur = this->child;
			while (cur && index > 0) {
				cur = cur->brother;
				--index;
			}
			return cur;
		}

		const Json* directChildByIndex(size_t index) const {
			Json* cur = this->child;
			while (cur && index > 0) {
				cur = cur->brother;
				--index;
			}
			return cur;
		}

		void removeDirectChildrenByKey(string_view key) {
			if (this->type != Type::Object || !this->child)
				return;
			Json* prev = nullptr;
			Json* cur = this->child;
			while (cur) {
				if (cur->name == key) {
					Json* doomed = cur;
					cur = cur->brother;
					if (prev)
						prev->brother = cur;
					else
						this->child = cur;
					doomed->brother = nullptr;
					deleteJson(doomed);
					continue;
				}
				prev = cur;
				cur = cur->brother;
			}
			this->lastChild = prev;
			if (!this->child)
				this->lastChild = nullptr;
			if (this->keymap) { delete this->keymap; this->keymap = nullptr; }
		}

		// Detaches `node` from `container`'s child chain. `prev` is the node that
		// precedes `node` in that chain, or nullptr when `node` is the first child.
		// Repairs the tail pointer (lastChild) and invalidates the lazy key index.
		static void unlinkChild(Json* container, Json* prev, Json* node) {
			if (prev)
				prev->brother = node->brother;
			else
				container->child = node->brother;
			node->brother = nullptr;
			if (container->lastChild == node)
				container->lastChild = prev;   // nullptr when the chain is now empty
			if (container->keymap) {
				delete container->keymap;
				container->keymap = nullptr;
			}
		}

		static bool decodePointerToken(const string& token, string& decoded) {
			decoded.clear();
			decoded.reserve(token.size());
			for (size_t i = 0; i < token.size(); ++i) {
				if (token[i] != '~') {
					decoded.push_back(token[i]);
					continue;
				}
				if (i + 1 >= token.size())
					return false;
				char next = token[++i];
				if (next == '0') decoded.push_back('~');
				else if (next == '1') decoded.push_back('/');
				else return false;
			}
			return true;
		}

		static bool parsePointerIndex(const string& token, size_t& index) {
			if (token.empty())
				return false;
			if (token.size() > 1 && token[0] == '0')
				return false;
			size_t value = 0;
			for (char ch : token) {
				if (ch < '0' || ch > '9')
					return false;
				value = value * 10 + static_cast<size_t>(ch - '0');
			}
			index = value;
			return true;
		}

		// Allocation-free token decode.  Without escapes the token is returned as a view
		// of the input; with escapes it is decoded into `buffer`, falling back to
		// `overflow` only for tokens longer than the buffer.
		static bool decodeTokenInto(string_view token, char* buffer, size_t capacity, string_view& decoded, string& overflow) {
			if (token.find('~') == string_view::npos) {
				decoded = token;
				return true;
			}

			if (token.size() <= capacity) {
				size_t written = 0;
				for (size_t i = 0; i < token.size(); ++i) {
					if (token[i] != '~') {
						buffer[written++] = token[i];
						continue;
					}
					if (i + 1 >= token.size())
						return false;
					const char next = token[++i];
					if (next == '0') buffer[written++] = '~';
					else if (next == '1') buffer[written++] = '/';
					else return false;
				}
				decoded = string_view(buffer, written);
				return true;
			}

			overflow.clear();
			overflow.reserve(token.size());
			for (size_t i = 0; i < token.size(); ++i) {
				if (token[i] != '~') {
					overflow.push_back(token[i]);
					continue;
				}
				if (i + 1 >= token.size())
					return false;
				const char next = token[++i];
				if (next == '0') overflow.push_back('~');
				else if (next == '1') overflow.push_back('/');
				else return false;
			}
			decoded = string_view(overflow.data(), overflow.size());
			return true;
		}

		static bool parsePointerIndexView(string_view token, size_t& index) {
			if (token.empty())
				return false;
			if (token.size() > 1 && token[0] == '0')
				return false;
			size_t value = 0;
			for (char ch : token) {
				if (ch < '0' || ch > '9')
					return false;
				value = value * 10 + static_cast<size_t>(ch - '0');
			}
			index = value;
			return true;
		}

		void overwritePreservingLinks(const Json& value, bool preserveName = true) {
			Json* next = this->brother;
			string savedName = this->name;
			*this = value;
			this->brother = next;
			if (preserveName)
				this->name = std::move(savedName);
		}

		// Writes one leaf value through a sink (string- or stream-backed).
		template <typename Sink>
		static void writeRawValue(const Json* node, Sink& sink) {
			switch (node->type) {
			case Type::String:
				sink.push_back('"');
				sink.appendEscaped(node->valueString.view());
				sink.push_back('"');
				break;
			case Type::Number:
				writeNumber(node, sink);
				break;
			case Type::True:
				sink.append("true", 4);
				break;
			case Type::False:
				sink.append("false", 5);
				break;
			case Type::Null:
				sink.append("null", 4);
				break;
			case Type::Error:
			case Type::Object:
			case Type::Array:
			default:
				break;
			}
		}

		// Compact printer.  Iterative, and templated on the sink so the same code can
		// fill a string or stream directly to an output stream without building the
		// whole document in memory first.
		template <typename Sink>
		static void writeCompact(const Json* json, Sink& sink) {
			if (json->type != Type::Object && json->type != Type::Array) {
				writeRawValue(json, sink);
				return;
			}

			struct Frame {
				const Json* container;
				const Json* next;
				bool wroteAny;
			};

			detail::SmallStack<Frame, 64> frames;
			sink.push_back(json->type == Type::Object ? '{' : '[');
			frames.push_back({ json, json->child, false });

			while (!frames.empty()) {
				Frame& frame = frames.back();
				if (!frame.next) {
					sink.push_back(frame.container->type == Type::Object ? '}' : ']');
					frames.pop_back();
					continue;
				}

				const Json* cur = frame.next;
				frame.next = cur->brother;
				if (frame.wroteAny)
					sink.push_back(',');
				else
					frame.wroteAny = true;

				if (frame.container->type == Type::Object) {
					sink.push_back('"');
					sink.appendEscaped(cur->name.view());
					sink.append("\":", 2);
				}

				if (cur->type == Type::Object || cur->type == Type::Array) {
					sink.push_back(cur->type == Type::Object ? '{' : '[');
					frames.push_back({ cur, cur->child, false });
				} else {
					writeRawValue(cur, sink);
				}
			}
		}

		void serializeCompact(const Json* json, string& result) const {
			detail::StringSink sink(result);
			writeCompact(json, sink);
		}

		// Pre-computed size hint for reserve().
		//
		// Two implementations behind one entry point.  The recursive one is the fast path
		// because ordinary documents nest a handful of levels and direct recursion is
		// cheaper than maintaining frames (measured: a frame-based walk was ~20% slower on
		// a 10k-member document, and routing every node through a dispatcher kept most of
		// that cost).  Past maxEstimateRecursion the walk switches to the frame-based
		// version, so a self-built deep tree is still measured without touching the stack.
		static constexpr int maxEstimateRecursion = 512;

		size_t estimateSerializedSize(const Json* json, int indentSize = 0, int depth = 0) const {
			if (!json)
				return 0;
			if (depth >= maxEstimateRecursion)
				return estimateSerializedSizeIterative(json, indentSize, depth);
			return estimateSerializedSizeRecursive(json, indentSize, depth);
		}

		size_t estimateSerializedSizeRecursive(const Json* json, int indentSize, int depth) const {
			switch (json->type) {
			case Type::Object:
			case Type::Array: {
				if (!json->child)
					return 2;
				const bool isObject = (json->type == Type::Object);
				size_t total = 2;
				size_t members = 0;
				for (const Json* cur = json->child; cur; cur = cur->brother) {
					if (isObject)
						total += cur->name.size() + 3;			// "key":
					if (indentSize > 0)
						total += static_cast<size_t>((depth + 1) * indentSize + 1);
					// Recurse directly: routing each node back through the entry point would
					// add a dispatch per node and stop the recursion being specialised.
					if (cur->type == Type::Object || cur->type == Type::Array) {
						if (depth + 1 < maxEstimateRecursion)
							total += estimateSerializedSizeRecursive(cur, indentSize, depth + 1);
						else
							total += estimateSerializedSizeIterative(cur, indentSize, depth + 1);
					} else {
						total += leafSize(cur);
					}
					++members;
				}
				if (members > 1)
					total += members - 1;						// separators
				if (indentSize > 0)
					total += static_cast<size_t>(depth * indentSize + 2);
				return total;
			}
			default:
				return leafSize(json);
			}
		}

		// Frame-based fallback: leaves contribute a constant, so only containers get a
		// frame, and each is descended into immediately - the stack depth tracks nesting
		// depth rather than the width of the document.
		size_t estimateSerializedSizeIterative(const Json* json, int indentSize, int depth) const {
			if (json->type != Type::Object && json->type != Type::Array)
				return leafSize(json);

			struct Frame {
				const Json* container;   // container being accounted for
				const Json* next;        // next member to account for
				size_t members;          // members seen so far (separators count at close)
				int depth;
			};

			size_t total = 0;
			detail::SmallStack<Frame, 64> containers;

			auto openContainer = [&](const Json* container, int containerDepth) {
				total += 2;                                     // braces
				if (indentSize > 0 && container->child)
					total += static_cast<size_t>(containerDepth * indentSize + 1);   // closing '\n' + indent
				containers.push_back({ container, container->child, 0, containerDepth });
			};

			openContainer(json, depth);

			while (!containers.empty()) {
				Frame& frame = containers.back();
				const Json* cur = frame.next;
				if (!cur) {
					if (frame.members > 1)
						total += frame.members - 1;             // separators
					containers.pop_back();
					continue;
				}

				// Update the frame completely before any push (push_back may reallocate).
				frame.next = cur->brother;
				++frame.members;

				const bool isObjectMember = (frame.container->type == Type::Object);
				if (isObjectMember)
					total += cur->name.size() + 3;              // "key":
				if (indentSize > 0)
					total += static_cast<size_t>((frame.depth + 1) * indentSize + 1);

				if (cur->type == Type::Object || cur->type == Type::Array)
					openContainer(cur, frame.depth + 1);
				else
					total += leafSize(cur);
			}
			return total;
		}

		static size_t leafSize(const Json* node) {
			switch (node->type) {
			case Type::False: return 5;
			case Type::True: return 4;
			case Type::Null: return 4;
			case Type::Number: return 32;		// generous upper bound for the shortest round-trip form
			case Type::String: return node->valueString.size() + 2;
			default: return 0;
			}
		}

		// Pretty printer.  Iterative, like the compact one, so that a deep document can
		// be printed without touching the call stack; each frame remembers where in its
		// member chain the writer stopped.
		template <typename Sink>
		static void writePretty(const Json* json, Sink& sink, int indentSize, int depth) {
			if (json->type != Type::Object && json->type != Type::Array) {
				writeRawValue(json, sink);
				return;
			}

			struct Frame {
				const Json* container;
				const Json* next;
				int depth;
				bool wroteAny;
			};

			detail::SmallStack<Frame, 64> stk;
			stk.push_back({ json, json->child, depth, false });
			sink.push_back(json->type == Type::Object ? '{' : '[');

			while (!stk.empty()) {
				Frame& frame = stk.back();
				if (!frame.next) {
					if (frame.wroteAny) {
						sink.push_back('\n');
						sink.appendIndent(indentSize, frame.depth);
					}
					sink.push_back(frame.container->type == Type::Object ? '}' : ']');
					stk.pop_back();
					continue;
				}

				const Json* cur = frame.next;
				frame.next = cur->brother;
				if (frame.wroteAny)
					sink.push_back(',');
				else
					frame.wroteAny = true;
				sink.push_back('\n');
				sink.appendIndent(indentSize, frame.depth + 1);

				if (frame.container->type == Type::Object) {
					sink.push_back('"');
					sink.appendEscaped(cur->name.view());
					sink.append("\": ", 3);
				}

				if (cur->type == Type::Object || cur->type == Type::Array) {
					sink.push_back(cur->type == Type::Object ? '{' : '[');
					stk.push_back({ cur, cur->child, frame.depth + 1, false });
				} else {
					writeRawValue(cur, sink);
				}
			}
		}

		// R1-1: equality.  Members are grouped by key so a lookup does not rescan the
		// whole right-hand side, and duplicate keys behave as a multiset: a key that
		// appears twice on the left must appear twice on the right, with matching
		// values in some pairing.  The traversal is iterative so deep documents can be
		// compared without recursing on the call stack.
		bool equalsIterative(const Json& other) const {
			detail::SmallStack<std::pair<const Json*, const Json*>, 64> work;
			work.push_back({ this, &other });

			while (!work.empty()) {
				const Json* lhs = work.back().first;
				const Json* rhs = work.back().second;
				work.pop_back();

				if (lhs->type != rhs->type)
					return false;

				switch (lhs->type) {
				case Type::Number:
					if (!numberEquals(*lhs, *rhs))
						return false;
					break;
				case Type::String:
					if (lhs->valueString != rhs->valueString)
						return false;
					break;
				case Type::Object:
					if (!compareObjects(*lhs, *rhs, work))
						return false;
					break;
				case Type::Array:
					if (!compareArrays(*lhs, *rhs, work))
						return false;
					break;
				case Type::Error:
				case Type::False:
				case Type::True:
				case Type::Null:
					break;										// same type is enough
				}
			}
			return true;
		}

		// Queues every member pair of two objects that still has to be compared deeply.
		//
		// Members are grouped by key first, so the ordinary shape (one member per key) is
		// linear.  The one quadratic shape left is many *duplicate* members of the same key,
		// all of the same type, that are not deeply equal: pairing them is a multiset
		// matching, and each member scans the still-unused candidates of its key.  That
		// shape is rare, the member counts already have to match, and the deep comparisons
		// it performs are the ones the result actually depends on - so it is documented
		// rather than bounded by another index (see the review's N4).
		template <typename WorkStack>
		bool compareObjects(const Json& lhs, const Json& rhs, WorkStack& work) const {
			const size_t lhsCount = lhs.childCount();
			if (lhsCount != rhs.childCount())
				return false;
			if (lhsCount == 0)
				return true;

			struct Candidate { const Json* node; bool used; };
			std::unordered_map<string_view, std::vector<Candidate>> byKey;
			byKey.reserve(lhsCount);
			for (const Json* cur = rhs.child; cur; cur = cur->brother)
				byKey[cur->name.view()].push_back({ cur, false });

			for (const Json* member = lhs.child; member; member = member->brother) {
				auto found = byKey.find(member->name.view());
				if (found == byKey.end())
					return false;
				std::vector<Candidate>& candidates = found->second;

				// Prefer a same-typed, not yet used candidate.  When only one candidate
				// remains the deep comparison can be deferred to the shared work list
				// (this is the common case and keeps deep chains iterative).
				size_t usable = 0;
				size_t lastUsable = 0;
				for (size_t i = 0; i < candidates.size(); ++i) {
					if (!candidates[i].used && candidates[i].node->type == member->type) {
						++usable;
						lastUsable = i;
					}
				}
				if (usable == 0)
					return false;

				if (usable == 1) {
					candidates[lastUsable].used = true;
					work.push_back({ member, candidates[lastUsable].node });
					continue;
				}

				// Several candidates share this key: pick the first one that is deeply
				// equal.  This nested comparison is iterative as well, and it only nests
				// as deep as the document nests duplicate keys.
				bool matched = false;
				for (Candidate& candidate : candidates) {
					if (candidate.used || candidate.node->type != member->type)
						continue;
					if (member->equalsIterative(*candidate.node)) {
						candidate.used = true;
						matched = true;
						break;
					}
				}
				if (!matched)
					return false;
			}
			return true;
		}

		// Queues the element pairs of two arrays, in order.
		template <typename WorkStack>
		bool compareArrays(const Json& lhs, const Json& rhs, WorkStack& work) const {
			const Json* left = lhs.child;
			const Json* right = rhs.child;
			while (left || right) {
				if (!left || !right)
					return false;							// different lengths
				work.push_back({ left, right });
				left = left->brother;
				right = right->brother;
			}
			return true;
		}

		// Parses `in`, which the arena takes ownership of.  The const& overload copies
		// the input; the std::string&& overload moves it, so parsing a buffer the caller
		// is finished with costs no copy.  There is deliberately no entry point that
		// borrows a caller-owned char buffer: the parsed document may keep views into
		// that buffer, and a borrowed view would dangle as soon as the caller reuses it.
		static Json parse(std::string&& in, std::string& err, const ParseOptions& options = ParseOptions{})
		{
			err.clear();
			if (options.validateUtf8) {
				size_t errPos = 0;
				if (!validate_utf8_bytes(in, errPos)) {
					err = "invalid UTF-8 byte at position " + std::to_string(errPos);
					return Json(Type::Error);
				}
			}
			const size_t inputSize = in.size();
			auto arena = std::make_shared<detail::StringArena>(std::move(in));
			JsonParser parser{ *arena->source, 0, err, false, options.allowComments, options.duplicateKey, arena };
			Json result = parser.parse_json_pda();
			if (result.type == Type::Error)
				return result;
			parser.consume_garbage();
			if (parser.i != inputSize)
				return parser.fail("unexpected trailing " + esc(arena->source->at(parser.i)));
			return result;
		}

		static Json parse(const std::string& in, std::string& err, const ParseOptions& options = ParseOptions{})
		{
			// The arena copies the input once; no temporary std::string is materialised in
			// between (that would add a second full-size allocation for every parse).
			err.clear();
			if (options.validateUtf8) {
				size_t errPos = 0;
				if (!validate_utf8_bytes(in, errPos)) {
					err = "invalid UTF-8 byte at position " + std::to_string(errPos);
					return Json(Type::Error);
				}
			}
			const size_t inputSize = in.size();
			auto arena = std::make_shared<detail::StringArena>(in);
			JsonParser parser{ *arena->source, 0, err, false, options.allowComments, options.duplicateKey, arena };
			Json result = parser.parse_json_pda();
			if (result.type == Type::Error)
				return result;
			parser.consume_garbage();
			if (parser.i != inputSize)
				return parser.fail("unexpected trailing " + esc(arena->source->at(parser.i)));
			return result;
		}

		static Json parse(const char* in, std::string& err, const ParseOptions& options = ParseOptions{}) {
			if (in) {
				return parse(std::string(in), err, options);
			}
			else {
				err = "null input";
				Json rs(Type::Error);
				return rs;
			}
		}

		static inline bool in_range(long x, long lower, long upper) {
			return (x >= lower && x <= upper);
		}

		Json(Type type) {
			this->brother = nullptr;
			this->child = nullptr;
			this->lastChild = nullptr;
			this->keymap = nullptr;
			this->type = type;
		}

		// Copies the scalar state of a node; the caller wires up the links.
		static Json* cloneScalar(const Json* source) {
			Json* node = new Json(source->type);
			node->name = source->name;
			node->valueString = source->valueString;
			node->numberKind = source->numberKind;
			node->valueNumber = source->valueNumber;
			return node;
		}

		// Clones `source` and its whole subtree, including the sibling chain the source
		// belongs to. *outTail receives the last node of the cloned chain.
		//
		// Iterative on purpose: the previous recursive version exhausted the call stack at
		// roughly 8000 levels of nesting (covered by the deep-document tests). Frames hold
		// one *open container* each and children are descended into immediately, so the
		// stack tracks nesting depth only - a container with 10k children must not pile up
		// 10k frames (measured: doing so cost 20% of copy time on a 1MB top-level array).
		static Json* cloneChain(const Json* source, Json** outTail = nullptr) {
			if (!source) {
				if (outTail) *outTail = nullptr;
				return nullptr;
			}

			struct Frame {
				const Json* src;    // container whose children are being cloned
				Json* dst;          // its clone (nullptr for the top-level chain)
				const Json* next;   // next child of `src` to clone
				Json* tail;         // last child appended to `dst`
			};

			std::vector<Frame> stack;
			stack.push_back({ nullptr, nullptr, source, nullptr });   // the top-level chain

			Json* head = nullptr;
			Json* tail = nullptr;

			while (!stack.empty()) {
				Frame& frame = stack.back();
				if (!frame.next) {
					stack.pop_back();
					continue;
				}

				const Json* cur = frame.next;
				Json* node = cloneScalar(cur);

				// Update the frame completely before pushing (push_back may reallocate).
				frame.next = cur->brother;
				if (!frame.dst) {                      // top-level chain
					if (tail)
						tail->brother = node;
					else
						head = node;
					tail = node;
				} else if (frame.tail) {
					frame.tail->brother = node;
					frame.tail = node;
					frame.dst->lastChild = node;
				} else {
					frame.dst->child = node;
					frame.tail = node;
					frame.dst->lastChild = node;
				}

				if (cur->child)
					stack.push_back({ cur, node, cur->child, nullptr });
			}

			if (outTail) *outTail = tail;
			return head;
		}

		void releaseChildren() {
			if (this->child) {
				deleteJson(this->child);
				this->child = nullptr;
			}
			this->lastChild = nullptr;
			if (this->keymap) { delete this->keymap; this->keymap = nullptr; }
		}

	public:
		using iterator = JsonIterator;
		using const_iterator = JsonConstIterator;

		static void* operator new(size_t size) {
			return detail::jsonNodeAllocator<sizeof(Json)>().allocate(size);
		}

		// The sized overload is preferred by the compiler for class-specific
		// deallocation, so the pool can tell slab blocks and oversized
		// (::operator new) blocks apart on any thread.
		static void operator delete(void* ptr, size_t size) noexcept {
			detail::jsonNodeAllocator<sizeof(Json)>().deallocate(ptr, size);
		}

		static void operator delete(void* ptr) noexcept {
			detail::jsonNodeAllocator<sizeof(Json)>().deallocate(ptr);
		}

		Json(JsonType type = JsonType::Object) {
			this->brother = nullptr;
			this->child = nullptr;
			this->lastChild = nullptr;
			this->keymap = nullptr;
			this->type = (Type)type;
		}

		// Integral sources keep their exact value (R5-1): a `long long` no longer
		// detours through a double, which is what used to round 2^53 + 1 on the way
		// in.  Signedness picks the state, so an `unsigned long long` above
		// INT64_MAX is still representable.
		template<typename T, typename std::enable_if<std::is_arithmetic<typename std::decay<T>::type>::value && !std::is_same<typename std::decay<T>::type, bool>::value && !std::is_same<typename std::decay<T>::type, float>::value && !std::is_same<typename std::decay<T>::type, double>::value, int>::type = 0> Json(const T& value) {
			this->brother = nullptr;
			this->child = nullptr;
			this->lastChild = nullptr;
			this->keymap = nullptr;
			if constexpr (std::is_signed<T>::value)
				this->setNumberInt64(static_cast<int64_t>(value));
			else
				this->setNumberUint64(static_cast<uint64_t>(value));
			this->type = Type::Number;
		}

		template<typename T, typename std::enable_if<!std::is_arithmetic<typename std::decay<T>::type>::value && detail::has_adl_to_json<typename std::decay<T>::type>::value, int>::type = 0> Json(const T& value) : Json(Type::Null) {
			to_json(*this, value);
		}

		Json(const float& value) {
			this->lastChild = nullptr;
			this->keymap = nullptr;
			if (std::isnan(value)) {
				this->brother = nullptr;
				this->child = nullptr;
				this->type = Type::Null;
			}
			else {
				this->brother = nullptr;
				this->child = nullptr;
				this->setNumberDouble(value);
				this->type = Type::Number;
			}
		}

		Json(const double& value) {
			this->lastChild = nullptr;
			this->keymap = nullptr;
			if (std::isnan(value)) {
				this->brother = nullptr;
				this->child = nullptr;
				this->type = Type::Null;
			}
			else {
				this->brother = nullptr;
				this->child = nullptr;
				this->setNumberDouble(value);
				this->type = Type::Number;
			}
		}

		Json(const string& jsonStr) : Json(Type::Error) {
			auto it = std::find_if_not(jsonStr.begin(), jsonStr.end(), [](unsigned char x) {return std::isspace(x); });
			if (it != jsonStr.end() && (*it == '{' || *it == '[')) {
				string err;
				*this = parse(jsonStr, err);
				if (this->isError()) {
					this->type = Type::String;
					this->valueString = jsonStr;
				}
			}
			else {
				this->type = Type::String;
				this->valueString = jsonStr;
			}
		}

		Json(const char* jsonStr) : Json(Type::Error) {
			if (jsonStr == nullptr) {
				this->type = Type::Null;
				return;
			}
			*this = Json(string(jsonStr));
		}

		Json(const bool& value) {
			this->brother = nullptr;
			this->child = nullptr;
			this->lastChild = nullptr;
			this->keymap = nullptr;
			this->type = value ? Type::True : Type::False;
		}

		Json(const std::nullptr_t&) {
			this->brother = nullptr;
			this->child = nullptr;
			this->lastChild = nullptr;
			this->keymap = nullptr;
			this->type = Type::Null;
		}

		Json(const Json& origin) {
			this->brother = nullptr;
			this->lastChild = nullptr;
			this->keymap = nullptr;
			this->type = origin.type;
			this->name = origin.name;
			this->valueString = origin.valueString;
			this->numberKind = origin.numberKind;
			this->valueNumber = origin.valueNumber;
			Json* childTail = nullptr;
			this->child = cloneChain(origin.child, &childTail);
			this->lastChild = childTail;
		}

		Json(Json&& rhs) noexcept {
			this->type = rhs.type;
			this->child = rhs.child;
			// A moved-to node is detached: inheriting rhs's sibling link would drag
			// the source chain into whatever container the node is linked into next.
			this->brother = nullptr;
			this->lastChild = rhs.lastChild;
			this->keymap = rhs.keymap;
			this->name = std::move(rhs.name);
			this->valueString = std::move(rhs.valueString);
			this->numberKind = rhs.numberKind;
			this->valueNumber = rhs.valueNumber;
			rhs.child = nullptr;
			rhs.brother = nullptr;
			rhs.lastChild = nullptr;
			rhs.keymap = nullptr;
		}

		explicit Json(std::initializer_list<std::pair<const std::string, Json>> values) {
			this->child = nullptr;
			this->brother = nullptr;
			this->lastChild = nullptr;
			this->keymap = nullptr;
			this->type = Type::Object;
			for (auto al : values) {
				al.second.name = al.first;
				this->extendItem(&al.second);
			}
		}

		// Reads and parses a file.  Document files ({...} / [...]) go through the
		// move-parsing entry point, so the buffer the file was read into becomes the
		// document's storage: a large configuration file is never copied a second time.
		// Anything else keeps the historical behaviour of becoming a string value.
		static Json FromFile(const char* filepath) {
			if (!filepath)
				return Json(Type::Error);
			std::ifstream file(filepath, std::ios::binary);
			if (!file.is_open())
				return Json(Type::Error);

			// One sized read instead of istreambuf_iterator<char>, which copied the file
			// byte by byte.  seekg/tellg gives the length for regular files; the stream
			// fallback covers the rare non-seekable case.
			file.seekg(0, std::ios::end);
			const std::streamoff size = file.tellg();
			if (size > 0) {
				std::string content(static_cast<size_t>(size), '\0');
				file.seekg(0, std::ios::beg);
				if (file.read(&content[0], size))
					return fromFileContent(std::move(content));
				return Json(Type::Error);
			}

			file.clear();
			file.seekg(0, std::ios::beg);
			std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
			if (content.empty())
				return Json(Type::Error);
			return fromFileContent(std::move(content));
		}

		// Dispatches file content to the cheapest correct entry point: a document is parsed
		// in place (one copy, inside the arena), anything else is stored as text.
		static Json fromFileContent(std::string&& content) {
			size_t first = 0;
			while (first < content.size()) {
				const char ch = content[first];
				if (ch != ' ' && ch != '\t' && ch != '\n' && ch != '\r')
					break;
				++first;
			}
			if (first < content.size() && (content[first] == '{' || content[first] == '[')) {
				std::string err;
				return parse(std::move(content), err);
			}
			return Json(content);
		}

		static Json FromFile(const std::string& filepath) {
			return Json::FromFile(filepath.c_str());
		}

		static Json ParseJson(const std::string& input, std::string& errMsg) {
			return parse(input, errMsg);
		}

		// Takes ownership of `input` (no copy of the document text).  The caller must not
		// use the string afterwards.
		static Json ParseJson(std::string&& input, std::string& errMsg) {
			return parse(std::move(input), errMsg);
		}

		static Json ParseJson(const std::string& input, std::string& errMsg, ParseOptions options) {
			return parse(input, errMsg, options);
		}

		static Json ParseJsonStrict(const std::string& input, std::string& errMsg) {
			ParseOptions options;
			options.allowComments = false;
			return parse(input, errMsg, options);
		}

		static Json ParseJsonStrict(const std::string& input, std::string& errMsg, ParseOptions options) {
			options.allowComments = false;
			return parse(input, errMsg, options);
		}

		static Json ParseJsonStrictUtf8(const std::string& input, std::string& errMsg) {
			ParseOptions options;
			options.allowComments = false;
			options.validateUtf8 = true;
			return parse(input, errMsg, options);
		}

		static Json ParseJsonStrictUtf8(const std::string& input, std::string& errMsg, ParseOptions options) {
			options.allowComments = false;
			options.validateUtf8 = true;
			return parse(input, errMsg, options);
		}

		~Json() {
			releaseChildren();
		}

		Json& operator = (const Json& origin) {
			if (this == &origin)
				return(*this);
			// A member node is a link in its owner's sibling chain, and it owns its key:
			// the assignment replaces the value in place, so the chain position and the
			// key must both survive. Clearing `brother` here (or taking the source's)
			// detached the node, silently dropping every following sibling of the owner -
			// which is why `for (auto& [k, v] : obj) v = Json(1);` used to keep only the
			// first member. The same rule is what overwritePreservingLinks() implements
			// for the patch paths.
			Json* savedBrother = this->brother;
			const bool keepName = !this->name.empty() && origin.name.empty();
			detail::StoredString savedName;
			if (keepName)
				savedName = this->name;
			releaseChildren();
			this->brother = savedBrother;
			this->type = origin.type;
			this->name = keepName ? savedName : origin.name;
			this->valueString = origin.valueString;
			this->numberKind = origin.numberKind;
			this->valueNumber = origin.valueNumber;
			Json* childTail = nullptr;
			this->child = cloneChain(origin.child, &childTail);
			this->lastChild = childTail;
			return(*this);
		}

		Json& operator = (Json&& rhs) noexcept {
			if (this == &rhs)
				return(*this);
			// See the copy assignment above: the chain position is part of this node's
			// identity, so it is restored rather than taken from `rhs`.
			Json* savedBrother = this->brother;
			const bool keepName = !this->name.empty() && rhs.name.empty();
			if (keepName) {
				detail::StoredString savedName = std::move(this->name);
				releaseChildren();
				this->type = rhs.type;
				this->child = rhs.child;
				this->brother = savedBrother;
				this->lastChild = rhs.lastChild;
				this->keymap = rhs.keymap;
				this->name = std::move(savedName);
				this->valueString = std::move(rhs.valueString);
				this->numberKind = rhs.numberKind;
				this->valueNumber = rhs.valueNumber;
				rhs.child = nullptr;
				rhs.brother = nullptr;
				rhs.lastChild = nullptr;
				rhs.keymap = nullptr;
				return(*this);
			}
			releaseChildren();
			this->type = rhs.type;
			this->child = rhs.child;
			this->brother = savedBrother;
			this->lastChild = rhs.lastChild;
			this->keymap = rhs.keymap;
			this->name = std::move(rhs.name);
			this->valueString = std::move(rhs.valueString);
			this->numberKind = rhs.numberKind;
			this->valueNumber = rhs.valueNumber;
			rhs.child = nullptr;
			rhs.brother = nullptr;
			rhs.lastChild = nullptr;
			rhs.keymap = nullptr;
			return(*this);
		}

		Json operator[](const int& index) const {
			Json rs(Type::Error);
			if (this->type == Type::Array) {
				if (index < 0 || this->child == nullptr) {
					return rs;
				}
				else {
					return this->child->find(index);
				}
			}
			else
				return rs;
		}

		// Direct member lookup through the lazy key index; nullptr when absent.
		//
		// The index compares each candidate node's own name against the requested key, so
		// no key is copied and nothing can dangle - a `string&` and a `string_view` caller
		// take exactly the same path.  (This used to be an unordered_map<string, Json*>,
		// which in C++17 has no heterogeneous lookup: a string_view caller had to build a
		// temporary string, and every indexed key was copied.  Measured 10x slower.)
		const Json* directMemberPtr(string_view key) const {
			if (this->type != Type::Object || !this->child)
				return nullptr;
			if (!this->keymap)
				buildKeymap();
			return this->keymap->find(key);
		}

		const Json* directMemberPtr(const string& key) const {
			return this->directMemberPtr(string_view(key.data(), key.size()));
		}

		// Resolves a key the way operator[] does: the direct member wins, otherwise the
		// deep-search fallback runs.  Returns a pointer into the document, or nullptr.
		// The two overloads keep the deep-search signature explicit; both take the same
		// path through the key index, which compares against each node's own name.
		template <typename Key>
		const Json* resolveMemberPtrWith(const Key& key) const {
			if (this->type != Type::Object || !this->child)
				return nullptr;
			if (const Json* direct = directMemberPtr(key))
				return direct;
			return findPtrDeep(key);
		}

		const Json* resolveMemberPtr(const string& key) const {
			return this->resolveMemberPtrWith(key);
		}

		const Json* resolveMemberPtr(string_view key) const {
			return this->resolveMemberPtrWith(key);
		}

		Json operator[](const string& key) const {
			const Json* found = resolveMemberPtr(key);
			return found ? *found : Json(Type::Error);
		}

		// Mutable pointer to `key` (direct member first, then the deep fallback).
		// Unlike operator[], which returns a copy, this gives direct write access.
		Json* findPtr(string_view key) {
			return const_cast<Json*>(static_cast<const Json&>(*this).resolveMemberPtr(key));
		}

		const Json* findPtr(string_view key) const {
			return resolveMemberPtr(key);
		}

		// Mutable pointer addressed by an RFC 6901 pointer; nullptr when it does not
		// resolve. See at() for the read-only value-returning form.
		Json* findPtrAt(string_view pointer) {
			return const_cast<Json*>(static_cast<const Json&>(*this).resolvePointerPtr(pointer));
		}

		const Json* findPtrAt(string_view pointer) const {
			return resolvePointerPtr(pointer);
		}

		bool contains(const string& key) const {
			if (this->type != Type::Object || !this->child) return false;
			if (!this->keymap) buildKeymap();
			return this->keymap->find(key) != nullptr;
		}

		string getValueType() const {
			return TYPENAMES[static_cast<int>(this->type)];
		}

		Json take(const string& key) {		//get and remove of Object
			Json rs = (*this)[key];
			this->remove(key);
			return rs;
		}

		Json take(const int& index) {		//get and remove of Array
			if (this->type == Type::Array) {
				Json rs(Type::Error);
				if (index >= 0 && index < this->size()) {
					rs = (*this)[index];
					(*this).remove(index);
				}
				return rs;
			}
			else
				return Json(Type::Error);
		}

		// Moves [start, end) out of the array.  end == 0 keeps its historical meaning of
		// "through the end".  Chains are walked once instead of calling take() per
		// element (which re-walked from the head and made this quadratic).
		Json takes(int start, int end = 0) {
			Json rs(Type::Array);
			if (this->type != Type::Array)
				return rs;
			if (end == 0)
				end = this->size();

			if (start < 0 || end <= start) {
				// Reproduce the historical result: the old loop fed operator[] one index
				// per iteration, and out-of-range indexes contributed an error node.
				for (int index = start; index < end; ++index)
					rs.push_back(Json(Type::Error));
				return rs;
			}

			Json* prev = nullptr;
			Json* cur = this->child;
			for (int index = 0; index < start && cur; ++index) {
				prev = cur;
				cur = cur->brother;
			}

			int remaining = end - start;
			while (cur && remaining > 0) {
				Json* next = cur->brother;
				unlinkChild(this, prev, cur);
				rs.push_back(std::move(*cur));
				delete cur;
				cur = next;
				--remaining;
			}
			// Past the end of the array: the old code appended error nodes for the
			// remaining iterations, so keep that observable behaviour.
			while (remaining-- > 0)
				rs.push_back(Json(Type::Error));
			return rs;
		}

		// Copies [start, end) out of the array in a single walk; end == 0 means "through
		// the end".  Out-of-range positions contribute an error node, matching
		// operator[](int).
		Json slice(int start, int end = 0) const {
			Json rs(Type::Array);
			if (this->type != Type::Array)
				return rs;
			if (end == 0)
				end = this->size();

			int index = 0;
			const Json* cur = this->child;
			for (; index < start && cur; ++index)
				cur = cur->brother;
			for (; index < end; ++index) {
				if (cur) {
					rs.push_back(*cur);
					cur = cur->brother;
				} else {
					rs.push_back(Json(Type::Error));
				}
			}
			return rs;
		}

		Json first() const {
			if (this->type == Type::Array && this->size() > 0)
				return (*this)[0];
			else
				return Json(Type::Error);
		}

		Json last() const {
			if (this->type == Type::Array && this->size() > 0)
				return (*this)[this->size() - 1];
			else
				return Json(Type::Error);
		}

		Json getAllKeys() const {
			Json rs(Type::Array);
			if (this->type == Type::Object) {
				Json* cur = this->child;
				while (cur) {
					if (cur->type != Type::Error)
						rs.push_back(cur->name.str());
					cur = cur->brother;
				}
			}
			return rs;
		}

		Json& add(std::initializer_list<Json> values) {
			if (this->type == Type::Array) {
				for (auto al : values)
					this->extendItem(&al);
			}
			return (*this);
		}

		template<typename T> Json& add(T value) {
			if (this->type == Type::Array)
				return this->addNamed("", Json(value));
			else
				return (*this);
		}

		Json& add(const Json& value) {
			if (this->type == Type::Array)
				return this->addNamed("", Json(value));
			else
				return (*this);
		}

		Json& add(Json&& value) {
			if (this->type == Type::Array)
				return this->addNamed("", std::move(value));
			else
				return (*this);
		}

		template<typename T> Json& add(string name, T value) {
			return this->addNamed(std::move(name), Json(value));
		}

		Json& add(string name, const Json& value) {
			return this->addNamed(std::move(name), Json(value));
		}

		Json& add(string name, Json&& value) {
			return this->addNamed(std::move(name), std::move(value));
		}

		bool isError() const noexcept {
			return this->type == Type::Error;
		}

		bool isNull() const noexcept {
			return this->type == Type::Null;
		}

		bool isObject() const noexcept {
			return this->type == Type::Object;
		}

		bool isArray() const noexcept {
			return this->type == Type::Array;
		}

		bool isNumber() const noexcept {
			return this->type == Type::Number;
		}

		bool isTrue() const noexcept {
			return this->type == Type::True;
		}

		bool isFalse() const noexcept {
			return this->type == Type::False;
		}

		bool isString() const noexcept {
			return this->type == Type::String;
		}

		int size() const noexcept {
			if (this->type == Type::Array) {
				int ct = 0;
				Json* cur = this->child;
				while (cur)
				{
					cur = cur->brother;
					ct++;
				}
				return ct;
			}
			else {
				return -1;
			}
		}

		bool isEmpty() const noexcept {
			return this->size() <= 0;
		}

		[[nodiscard]] string toString() const {
			if (this->type == Type::Error) {
				return "";
			}
			if (this->type == Type::String) {
				return this->valueString;
			}

			string result;
			result.reserve(estimateSerializedSize(this));
			detail::StringSink sink(result);
			writeCompact(this, sink);
			return result;
		}

		[[nodiscard]] string toString(int indent) const {
			if (indent <= 0)
				return this->toString();
			if (this->type == Type::Error)
				return "";
			string result;
			result.reserve(estimateSerializedSize(this, indent));
			detail::StringSink sink(result);
			writePretty(this, sink, indent, 0);
			return result;
		}

		// Writes the document straight to a stream instead of building the whole text
		// in memory first (dump()/operator<< keep their historical behaviour).
		std::ostream& dumpTo(std::ostream& out, int indent = 0) const {
			detail::StreamSink sink(out);
			if (indent > 0)
				writePretty(this, sink, indent, 0);
			else
				writeCompact(this, sink);
			return out;
		}

		std::ostream& dump(std::ostream& out, int indent = 0) const {
			out << (indent > 0 ? this->toString(indent) : this->toString());
			return out;
		}

		friend std::ostream& operator<<(std::ostream& out, const Json& json) {
			return json.dump(out);
		}

		// Reads the node that an RFC 6901 pointer addresses, or nullptr.  Tokens are
		// sliced as views and escapes are decoded into a stack buffer, so evaluating a
		// pointer does not build temporary std::strings (short tokens also stay inside
		// the key index's small-string buffer when it is probed).
		const Json* resolvePointerPtr(string_view pointer) const {
			if (pointer.empty())
				return this;
			if (pointer[0] != '/')
				return nullptr;

			char buffer[128];
			string overflow;
			const Json* current = this;
			size_t start = 1;
			for (;;) {
				const size_t slash = pointer.find('/', start);
				const string_view raw = pointer.substr(start, slash == string_view::npos ? string_view::npos : slash - start);
				string_view token;
				if (!decodeTokenInto(raw, buffer, sizeof(buffer), token, overflow))
					return nullptr;

				if (current->type == Type::Object) {
					const Json* direct = current->directMemberPtr(token);
					current = direct;
				} else if (current->type == Type::Array) {
					size_t index = 0;
					if (!parsePointerIndexView(token, index))
						return nullptr;
					current = current->directChildByIndex(index);
				} else {
					return nullptr;
				}

				if (!current)
					return nullptr;
				if (slash == string_view::npos)
					break;
				start = slash + 1;
			}
			return current;
		}

		Json at(const string& pointer) const {
			const Json* found = resolvePointerPtr(pointer);
			return found ? *found : Json(Type::Error);
		}

		// Read-only reference to the node a pointer addresses.  The returned reference
		// is the error sentinel when the pointer does not resolve, so callers that need
		// to distinguish use findPtrAt()/at() instead.
		const Json& atRef(string_view pointer) const {
			if (const Json* found = resolvePointerPtr(pointer))
				return *found;
			return errorSentinel();
		}

		// Stable node handed out by reference for "not found".  One per thread, so two
		// threads never observe each other's sentinel.
		//
		// It is allocated once and deliberately never destroyed: a thread_local object
		// would hand its block back to the thread's node pool at thread exit, and the
		// destruction order between the sentinel and that pool is not something this
		// library can rely on.  One node per thread for the process lifetime is the same
		// trade-off the pool itself makes.  Callers must not write through the reference -
		// it is const, and every failed lookup on this thread shares it.
		static const Json& errorSentinel() {
			static thread_local const Json* sentinel = new Json(Type::Error);
			return *sentinel;
		}

		// ---------------------------------------------------------------------------
		// Non-throwing accessors.  The target is only written when the lookup succeeds,
		// and the requested kind must match the stored one (a string is never parsed for
		// try_get(int&) - use toInt() when that conversion is what you want).
		// ---------------------------------------------------------------------------
		template <typename T>
		bool try_get(const string& key, T& out) const {
			const Json* found = resolveMemberPtr(key);
			if (!found)
				return false;

			if constexpr (std::is_same<T, bool>::value) {
				if (!found->isTrue() && !found->isFalse())
					return false;
				out = found->isTrue();
				return true;
			} else if constexpr (std::is_same<T, string>::value) {
				if (!found->isString())
					return false;
				out = found->valueString.str();
				return true;
			} else if constexpr (std::is_arithmetic<T>::value) {
				if (!found->isNumber())
					return false;
				// Converts from the stored state, so an int64 target keeps every bit of
				// a value that a double detour would have rounded.
				out = found->numberAs<T>();
				return true;
			} else if constexpr (detail::has_adl_from_json<T>::value) {
				T value{};
				from_json(*found, value);
				out = std::move(value);
				return true;
			} else {
				static_assert(detail::has_adl_from_json<T>::value,
					"try_get needs an arithmetic type, std::string, bool, or a type with an ADL from_json");
				return false;
			}
		}

		std::optional<int> try_int(const string& key) const {
			int value = 0;
			return try_get(key, value) ? std::optional<int>(value) : std::nullopt;
		}

		std::optional<double> try_double(const string& key) const {
			double value = 0;
			return try_get(key, value) ? std::optional<double>(value) : std::nullopt;
		}

		std::optional<string> try_string(const string& key) const {
			string value;
			return try_get(key, value) ? std::optional<string>(std::move(value)) : std::nullopt;
		}

		// ---------------------------------------------------------------------------
		// Pointer writes.  Delegates to the JSON Patch engine so that creation,
		// replacement and array-append ("-") follow RFC 6902 "add" exactly; the document
		// is only modified when the operation succeeds, so a failing setAt leaves it
		// untouched.
		// ---------------------------------------------------------------------------
		bool setAt(const string& pointer, const Json& value, string& err) {
			err.clear();
			Json operation;
			operation.add("op", "add");
			operation.add("path", pointer);
			operation.add("value", value);

			Json operations(JsonType::Array);
			operations.add(std::move(operation));

			Json result = applyPatch(operations, err);
			if (result.isError())
				return false;
			*this = std::move(result);
			return true;
		}

		bool setAt(const string& pointer, const Json& value) {
			string err;
			return setAt(pointer, value, err);
		}

		// Constructs an array from values.  (Brace construction such as `Json{...}` is
		// deliberately left to the object form, so no ambiguity is introduced.)
		static Json array(std::initializer_list<Json> values) {
			Json result(JsonType::Array);
			for (const Json& value : values)
				result.add(value);
			return result;
		}

		bool operator==(const Json& other) const {
			if (this->type != other.type)
				return false;
			// Reject unequal containers before any comparison bookkeeping is created, so a
			// size mismatch costs one walk per side and no allocation.
			if (this->type == Type::Object && this->childCount() != other.childCount())
				return false;
			return equalsIterative(other);
		}

		bool operator!=(const Json& other) const {
			return !(*this == other);
		}

		Json& mergePatch(const Json& patch) {
			if (!patch.isObject()) {
				*this = patch;
				return *this;
			}

			if (!this->isObject()) {
				*this = Json(JsonType::Object);
			}

			for (Json* cur = patch.child; cur; cur = cur->brother) {
				if (cur->isNull()) {
					this->removeDirectChildrenByKey(cur->name);
					continue;
				}

				Json* target = this->directChildByKey(cur->name);
				if (cur->isObject()) {
					Json merged = target ? *target : Json(JsonType::Object);
					merged.mergePatch(*cur);
					if (target)
						target->overwritePreservingLinks(merged, true);
					else
						this->add(cur->name.str(), merged);
					continue;
				}

				if (target)
					target->overwritePreservingLinks(*cur, true);
				else
					this->add(cur->name.str(), *cur);
			}
			return *this;
		}

		Json applyPatch(const Json& operations, string& err) const {
			err.clear();
			if (!operations.isArray()) {
				err = "JSON Patch document must be an array";
				return Json(Type::Error);
			}

			Json result = *this;

			auto makeError = [&](const string& message) -> Json {
				err = message;
				Json failure(Type::Error);
				failure.name = message;
				return failure;
			};

			auto resolveParent = [&](Json& root, const string& path, bool allowAppend, Json*& parent, string& token, bool& appendToArray, size_t& index) -> bool {
				appendToArray = false;
				index = 0;
				if (path.empty()) {
					parent = nullptr;
					token.clear();
					return true;
				}
				if (path[0] != '/') {
					err = "JSON Pointer must start with '/'";
					return false;
				}

				Json* current = &root;
				size_t start = 1;
				while (true) {
					size_t slash = path.find('/', start);
					string raw = path.substr(start, slash == string::npos ? string::npos : slash - start);
					string decoded;
					if (!decodePointerToken(raw, decoded)) {
						err = "invalid JSON Pointer escape in path '" + path + "'";
						return false;
					}
					if (slash == string::npos) {
						parent = current;
						token = std::move(decoded);
						if (current->isArray()) {
							if (allowAppend && token == "-") {
								appendToArray = true;
								index = static_cast<size_t>(current->size());
								return true;
							}
							if (!parsePointerIndex(token, index)) {
								err = "invalid array index '" + token + "' in path '" + path + "'";
								return false;
							}
						}
						return true;
					}

					if (current->isObject()) {
						current = current->directChildByKey(decoded);
					}
					else if (current->isArray()) {
						size_t childIndex = 0;
						if (!parsePointerIndex(decoded, childIndex)) {
							err = "invalid array index '" + decoded + "' in path '" + path + "'";
							return false;
						}
						current = current->directChildByIndex(childIndex);
					}
					else {
						err = "path '" + path + "' traverses a non-container node";
						return false;
					}

					if (!current) {
						err = "path '" + path + "' does not exist";
						return false;
					}
					start = slash + 1;
				}
			};

			auto resolveTarget = [&](Json& root, const string& path, Json*& target, Json*& parent, string& token, size_t& index) -> bool {
				bool appendToArray = false;
				if (path.empty()) {
					target = &root;
					parent = nullptr;
					token.clear();
					index = 0;
					return true;
				}
				if (!resolveParent(root, path, false, parent, token, appendToArray, index))
					return false;
				if (!parent) {
					target = &root;
					return true;
				}
				if (parent->isObject()) {
					target = parent->directChildByKey(token);
				}
				else if (parent->isArray()) {
					target = parent->directChildByIndex(index);
				}
				else {
					err = "path '" + path + "' parent is not a container";
					return false;
				}
				if (!target) {
					err = "path '" + path + "' does not exist";
					return false;
				}
				return true;
			};

			auto addValueAtPath = [&](Json& root, const string& path, const Json& value) -> bool {
				Json* parent = nullptr;
				string token;
				bool appendToArray = false;
				size_t index = 0;
				if (!resolveParent(root, path, true, parent, token, appendToArray, index))
					return false;
				if (!parent) {
					root = value;
					return true;
				}
				if (parent->isObject()) {
					Json* existing = parent->directChildByKey(token);
					if (existing)
						existing->overwritePreservingLinks(value, true);
					else
						parent->add(token, value);
					return true;
				}
				if (!parent->isArray()) {
					err = "path '" + path + "' parent is not a container";
					return false;
				}
				const size_t arraySize = static_cast<size_t>(parent->size());
				if (appendToArray || index == arraySize) {
					parent->push_back(value);
					return true;
				}
				if (index > arraySize) {
					err = "array index out of bounds in path '" + path + "'";
					return false;
				}
				parent->insert(static_cast<int>(index), value);
				return true;
			};

			auto removeAtPath = [&](Json& root, const string& path) -> bool {
				Json* target = nullptr;
				Json* parent = nullptr;
				string token;
				size_t index = 0;
				if (!resolveTarget(root, path, target, parent, token, index))
					return false;
				if (!parent) {
					err = "removing the document root is not supported";
					return false;
				}
				if (parent->isObject())
					parent->removeDirectChildrenByKey(token);
				else
					parent->remove(static_cast<int>(index));
				return true;
			};

			auto replaceAtPath = [&](Json& root, const string& path, const Json& value) -> bool {
				Json* target = nullptr;
				Json* parent = nullptr;
				string token;
				size_t index = 0;
				if (!resolveTarget(root, path, target, parent, token, index))
					return false;
				if (!parent) {
					root = value;
					return true;
				}
				target->overwritePreservingLinks(value, parent->isObject());
				return true;
			};

			auto isDescendantMove = [](const string& from, const string& path) -> bool {
				if (from.empty())
					return !path.empty();
				if (path.size() <= from.size())
					return false;
				return path.compare(0, from.size(), from) == 0 && path[from.size()] == '/';
			};

			for (Json* operation = operations.child; operation; operation = operation->brother) {
				if (!operation->isObject())
					return makeError("each JSON Patch operation must be an object");

				Json* opField = operation->directChildByKey("op");
				Json* pathField = operation->directChildByKey("path");
				if (!opField || !opField->isString())
					return makeError("JSON Patch operation is missing string field 'op'");
				if (!pathField || !pathField->isString())
					return makeError("JSON Patch operation is missing string field 'path'");

				const string op = opField->valueString;
				const string path = pathField->valueString;

				if (op == "add") {
					Json* valueField = operation->directChildByKey("value");
					if (!valueField)
						return makeError("add operation requires field 'value'");
					if (!addValueAtPath(result, path, *valueField))
						return makeError(err);
				}
				else if (op == "remove") {
					if (!removeAtPath(result, path))
						return makeError(err);
				}
				else if (op == "replace") {
					Json* valueField = operation->directChildByKey("value");
					if (!valueField)
						return makeError("replace operation requires field 'value'");
					if (!replaceAtPath(result, path, *valueField))
						return makeError(err);
				}
				else if (op == "move" || op == "copy") {
					Json* fromField = operation->directChildByKey("from");
					if (!fromField || !fromField->isString())
						return makeError(op + " operation requires string field 'from'");
					const string from = fromField->valueString;
					if (op == "move" && isDescendantMove(from, path))
						return makeError("move destination cannot be inside source path");

					Json* source = nullptr;
					Json* sourceParent = nullptr;
					string sourceToken;
					size_t sourceIndex = 0;
					if (!resolveTarget(result, from, source, sourceParent, sourceToken, sourceIndex))
						return makeError(err);
					Json movedValue = *source;
					if (op == "move" && !removeAtPath(result, from))
						return makeError(err);
					if (!addValueAtPath(result, path, movedValue))
						return makeError(err);
				}
				else if (op == "test") {
					Json* valueField = operation->directChildByKey("value");
					if (!valueField)
						return makeError("test operation requires field 'value'");
					Json* target = nullptr;
					Json* parent = nullptr;
					string token;
					size_t index = 0;
					if (!resolveTarget(result, path, target, parent, token, index))
						return makeError(err);
					if (!(*target == *valueField))
						return makeError("test operation failed at path '" + path + "'");
				}
				else {
					return makeError("unsupported JSON Patch operation '" + op + "'");
				}
			}

			return result;
		}

		iterator begin();
		iterator end();
		const_iterator begin() const;
		const_iterator end() const;
		const_iterator cbegin() const;
		const_iterator cend() const;

		int toInt() const {
			if (this->type == Type::Number)
				return this->numberAs<int>();
			return static_cast<int>(this->toDouble());
		}

		// Exact 64-bit reads (R5-1).  For an integer node this is the stored value
		// itself, so `9007199254740993` comes back intact; for a double or a string
		// it follows the same rule as a C++ cast, mirroring toInt()/toDouble().
		int64_t toInt64() const {
			if (this->type == Type::Number)
				return this->numberAs<int64_t>();
			return static_cast<int64_t>(this->toDouble());
		}

		uint64_t toUint64() const {
			if (this->type == Type::Number)
				return this->numberAs<uint64_t>();
			return static_cast<uint64_t>(this->toDouble());
		}

		// True when the node holds a Number that was stored as an integer, i.e. a
		// JSON integer literal (or an integral C++ scalar) that fits int64/uint64.
		// `42.0` and `42e0` are numbers but not integral; toInt64() still converts
		// them.
		bool isIntegral() const noexcept {
			return this->type == Type::Number && this->numberKind != detail::NumberKind::Double;
		}

		double toDouble() const {
			if (this->type == Type::Number)
				return this->numberAs<double>();
			else if (this->isTrue())
				return 1;
			else if (this->isFalse())
				return 0;
			else if (this->type == Type::String) {
				return detail::parseLeadingDouble(this->valueString.view());
			}
			else
				return 0;
		}

		float toFloat() const {
			return (float)this->toDouble();
		}

		template<typename T, typename std::enable_if<detail::has_adl_from_json<T>::value, int>::type = 0>
		T get() const {
			T value{};
			from_json(*this, value);
			return value;
		}

		bool toBool() const {
			if (this->type == Type::False || this->type == Type::True) {
				if (this->type == Type::True)
					return true;
				else
					return false;
			}
			else
				return false;
		}

		std::vector<Json> toVector() const {
			std::vector<Json> rs;
			if (this->type == Type::Array) {
				Json* cur = this->child;
				while (cur) {
					rs.push_back(*cur);
					cur = cur->brother;
				};
			}
			return rs;
		}

		Json& extend(Json value) {
			if (this->type == Type::Object && value.type == Type::Object) {
				Json* cur = value.child;
				while (cur) {
					Json* next = cur->brother;
					this->removeDirectChildrenByKey(cur->name);
					this->extendItem(cur);
					cur = next;
				}
			}
			return (*this);
		}

		Json& concat(Json value) {
			if (this->type == Type::Array)
			{
				if (value.type == Type::Array || value.type == Type::Object)
				{
					Json* cur = value.child;
					while (cur)
					{
						Json* next = cur->brother;
						this->extendItem(cur);
						cur = next;
					}
				}
				else {
					this->extendItem(&value);
				}
			}
			return (*this);
		}

		Json& push_front(const Json& value) {
			if (this->type == Type::Array)
			{
				Json* theChild = this->child;
				this->child = new Json(value);
				this->child->brother = theChild;
				if (!theChild) this->lastChild = this->child; // was empty
				theChild = nullptr;
			}
			return (*this);
		}
		Json& push_front(Json&& value) {
			if (this->type == Type::Array)
			{
				Json* theChild = this->child;
				this->child = new Json(std::move(value));
				this->child->brother = theChild;
				if (!theChild) this->lastChild = this->child; // was empty
				theChild = nullptr;
			}
			return (*this);
		}

		Json& push_back(const Json& value) {
			if (this->type == Type::Array)
				return add(value);
			else
				return (*this);
		}
		Json& push_back(Json&& value) {
			return add(std::move(value));
		}

		inline Json& push(const Json& value) {
			return this->push_back(value);
		}
		inline Json& push(Json&& value) {
			return this->push_back(std::move(value));
		}

		Json& pop_front() {
			if (this->type == Type::Array)
				return this->removeFirst();
			else
				return *this;
		}

		Json& pop_back() {
			if (this->type == Type::Array)
				return this->removeLast();
			else
				return *this;
		}
		inline Json& pop() {
			return this->pop_back();
		}

		Json& insert(int index, const Json& value) {
			if (this->type != Type::Array)
				return (*this);
			const int count = this->size();
			if (index < 0) {
				index += count;
				if (index < 0)
					return (*this);
			}
			if (index == 0)
				return this->push_front(value);
			if (index > count)
				return (*this);			// out of index range: no-op (historical behaviour)
			if (index == count)
				return this->push_back(value);	// append at the tail
			int ct = 0;
			Json* pre = this;
			Json* cur = this->child;
			while (cur && index != ct++) {
				pre = cur;
				cur = cur->brother;
			}
			// 0 < index < count, so cur != nullptr and the tail pointer is unchanged
			if (cur) {
				pre->brother = new Json(value);
				pre->brother->brother = cur;
			}
			return (*this);
		}
		Json& insert(int index, Json&& value) {
			if (this->type != Type::Array)
				return (*this);
			const int count = this->size();
			if (index < 0) {
				index += count;
				if (index < 0)
					return (*this);
			}
			if (index == 0)
				return this->push_front(std::move(value));
			if (index > count)
				return (*this);			// out of index range: no-op (historical behaviour)
			if (index == count)
				return this->push_back(std::move(value));	// append at the tail
			int ct = 0;
			Json* pre = this;
			Json* cur = this->child;
			while (cur && index != ct++) {
				pre = cur;
				cur = cur->brother;
			}
			if (cur) {
				pre->brother = new Json(std::move(value));
				pre->brother->brother = cur;
			}
			return (*this);
		}

		Json& clear() {
			if (this->type == Type::Array || this->type == Type::Object) {
				if (this->child)
					deleteJson(this->child);
				this->child = nullptr;
				this->lastChild = nullptr;
				if (this->keymap) { delete this->keymap; this->keymap = nullptr; }
			}
			return (*this);
		}

		// Removes every descendant whose key equals `key` (pre-order, at any
		// depth). The node itself is never removed, so a document root survives.
		// The walk uses an explicit stack: a deeply nested document cannot
		// overflow the call stack. Every unlink repairs both the sibling chain and
		// the parent's lastChild tail, and invalidates the parent's key index.
		//
		// `self` / `prev` keep the historical three-argument form working: they
		// start the scan at `self` with `prev` as the node preceding it (nullptr
		// meaning "first child of the owning container").
		Json& remove(const string& key, Json* self = nullptr, Json* prev = nullptr)
		{
			if (self == nullptr && this->type != Type::Object && this->type != Type::Array)
				return (*this);

			Json* start = (self == nullptr) ? this->child : self;
			if (!start)
				return (*this);

			// The three-argument form is the historical recursion shape: the caller passes
			// the owning container as `prev` together with its first child as `self` (the
			// old parser called remove(key, cur->child, cur)).  The owner therefore has to
			// be derived from `prev`; assuming `this` would unlink from the wrong chain,
			// splice unrelated nodes together and corrupt the document.  When `prev` is
			// null the call is the ordinary public one and `this` owns the chain.
			Json* container = prev ? prev : this;

			struct Frame {
				Json* container;   // owner of the chain currently being scanned
				Json* prev;        // predecessor of `cur` (nullptr => first child)
				Json* cur;         // next node to inspect
			};

			detail::SmallStack<Frame, 64> stk;
			stk.push_back({ container, nullptr, start });

			while (!stk.empty()) {
				Frame& frame = stk.back();
				if (!frame.cur) {
					stk.pop_back();
					continue;
				}

				Json* node = frame.cur;
				// Only object members carry a key: array elements are name-less, so
				// they are never candidates (matches the "[]" addressing rules).
				if (frame.container->type == Type::Object && node->name == key) {
					frame.cur = node->brother;      // advance before unlinking
					unlinkChild(frame.container, frame.prev, node);
					deleteJson(node);               // frees the node and its subtree
					continue;
				}

				if ((node->type == Type::Object || node->type == Type::Array) && node->child) {
					// Descend, but remember where to continue this chain. Update the
					// current frame *before* pushing (push_back may reallocate).
					frame.prev = node;
					frame.cur = node->brother;
					stk.push_back({ node, nullptr, node->child });
					continue;
				}

				frame.prev = node;
				frame.cur = node->brother;
			}
			return (*this);
		}

		Json& remove(const int& index) {
			if (this->type == Type::Array) {
				if (index == 0)
					return this->removeFirst();
				else if (index > 0 && index < this->size()) {
					int ct = 0;
					Json* pre = this;
					Json* cur = this->child;
					while (cur) {
						if (index == ct++)
							break;
						pre = cur;
						cur = cur->brother;
					}
					pre->brother = cur->brother;
					if (!cur->brother) this->lastChild = (pre == (Json*)this) ? nullptr : pre;
					cur->brother = nullptr;
					delete cur;
				}
			}
			return (*this);
		}

		Json& removeFirst() {
			if (this->type == Type::Array && this->size() > 0) {
				auto cur = this->child;
				this->child = cur->brother;
				if (!this->child) this->lastChild = nullptr; // was the only element
				cur->brother = nullptr;
				delete cur;
			}
			return *this;
		}

		Json& removeLast() {
			if (this->type == Type::Array)
				return this->remove(this->size() - 1);
			else
				return *this;
		}

		int indexOf(string value) {
			if (this->type != Type::Array)
				return -1;
			int ct = 0;
			for (Json* cur = this->child; cur; cur = cur->brother, ++ct) {
				if (serializedLeafEquals(cur, value))
					return ct;
			}
			return -1;
		}

	private:
		void extendItem(Json* cur) {
			const string childName = cur->name.str();
			switch (cur->type)
			{
			case Type::False:
				this->add(childName, false);
				break;
			case Type::True:
				this->add(childName, true);
				break;
			case Type::Null:
				this->add(childName, nullptr);
				break;
			case Type::Number:
				// Re-add the node itself so the numeric state (and therefore the exact
				// value) travels with it instead of being re-read from the double slot.
				this->add(childName, *cur);
				break;
			case Type::String:
				this->add(childName, cur->valueString.str());
				break;
			case Type::Object:
			case Type::Array:
				// Move the subtree instead of deep-copying it. Every caller owns the node
				// it passes here (extend/concat take the source by value, the
				// initializer-list constructor works on a local copy).
				this->add(childName, std::move(*cur));
				break;
			default:
				break;
			}
		}

		// Inserts `node` as a child. Objects honour the key verbatim - including the
		// empty key that parsing can produce ({"":1}) - and arrays ignore the key.
		// Callers that pass no key use the unnamed add() overloads, which already
		// restrict themselves to arrays.
		Json& addNamed(string name, Json&& node) {
			if (this->type != Type::Object && this->type != Type::Array)
				return (*this);
			Json* heapNode = new Json(std::move(node));
			heapNode->name = (this->type == Type::Object) ? std::move(name) : string();
			appendNodeToJson(heapNode);
			return (*this);
		}

		void appendNodeToJson(Json* node, Json* self = nullptr)
		{
			if (self == nullptr)
				self = this;
			// The incoming node is detached from any previous chain before it becomes
			// the new tail; otherwise a stale sibling link would splice a foreign list
			// into this container.
			node->brother = nullptr;
			// O(1) append using lastChild tail pointer
			if (self->lastChild) {
				self->lastChild->brother = node;
				self->lastChild = node;
			} else if (self->child) {
				// lastChild lost (shouldn't normally happen); fall back to traversal
				Json* prev = self->child;
				while (prev->brother) prev = prev->brother;
				prev->brother = node;
				self->lastChild = node;
			} else {
				self->child = node;
				self->lastChild = node;
			}

			// Keep the lazy key index in step with the chain. Appending used to drop the
			// whole index, which made add/lookup interleaving quadratic; a single insert
			// has the same meaning as rebuilding it (later duplicates win) and is O(1).
			if (self->keymap && self->type == Type::Object)
				self->keymap->assign(node->name.view(), node);
		}

		// Releases a node and its whole subtree. The block each node came from returns to
		// this thread's pool, so this is deliberately not recursive: a deep document must
		// be destroyable without growing the call stack. Each frame is a position in a
		// sibling chain and children are released immediately, so the stack follows
		// nesting depth rather than the width of a chain.
		static void deleteJson(Json* obj) {
			if (!obj)
				return;

			struct Frame {
				Json* next;     // next node of this chain to release
			};

			detail::SmallStack<Frame, 64> stack;
			stack.push_back({ obj });

			while (!stack.empty()) {
				Frame& frame = stack.back();
				Json* cur = frame.next;
				if (!cur) {
					stack.pop_back();
					continue;
				}

				// Advance before releasing: `cur` is about to be handed back to the pool.
				frame.next = cur->brother;
				if ((cur->type == Type::Object || cur->type == Type::Array) && cur->child)
					stack.push_back({ cur->child });

				if (cur->keymap) { delete cur->keymap; cur->keymap = nullptr; }
				cur->child = nullptr;
				cur->brother = nullptr;
				delete cur;
			}
		}

		// Lazily build a key index of all immediate children (Object keys only).
		// Empty keys are indexed too: {"":1} is a legal document and must be
		// reachable through operator[]/contains as well as through at("/").
		// A duplicate key keeps the LAST member, which is what the map's operator[]
		// did as it walked the chain in order.
		void buildKeymap() const {
			if (keymap) { delete keymap; keymap = nullptr; }
			keymap = new detail::JsonKeyIndex<Json>();
			keymap->reserve(childCount());
			Json* cur = child;
			while (cur) {
				keymap->assign(cur->name.view(), cur);
				cur = cur->brother;
			}
		}

		Json find(int index) {
			int ct = 0;
			Json* cur = this;
			Json rs(Type::Error);
			while (cur && ct < index)
			{
				cur = cur->brother;
				ct++;
			}
			if (ct < index || !cur)
				return rs;
			else
				return *cur;
		}

		// Deep-search fallback used by operator[]/findPtr when the direct key index
		// misses: the first *object member* whose key equals `key`, in document order
		// (pre-order: a container is explored before the siblings that follow it).
		// Array elements have no key, so they can never match - which also keeps the
		// empty key unambiguous.  Iterative, so deep documents cannot overflow the
		// stack, and a frame remembers where in a member chain the walk stopped.
		const Json* findPtrDeep(string_view key) const {
			struct Frame {
				const Json* container;
				const Json* next;
			};

			detail::SmallStack<Frame, 64> stk;
			stk.push_back({ this, this->child });
			while (!stk.empty()) {
				Frame& frame = stk.back();
				if (!frame.next) {
					stk.pop_back();
					continue;
				}

				const Json* child = frame.next;
				frame.next = child->brother;

				if (frame.container->type == Type::Object && child->name == key)
					return child;
				if ((child->type == Type::Object || child->type == Type::Array) && child->child)
					stk.push_back({ child, child->child });
			}
			return nullptr;
		}

		Json find(const string& key) const {
			const Json* found = findPtrDeep(key);
			return found ? *found : Json(Type::Error);
		}

		static inline string esc(char c) {
			char buf[12];
			if (static_cast<uint8_t>(c) >= 0x20 && static_cast<uint8_t>(c) <= 0x7f) {
				snprintf(buf, sizeof buf, "'%c' (%d)", c, c);
			}
			else {
				snprintf(buf, sizeof buf, "(%d)", c);
			}
			return string(buf);
		}

		struct JsonParser final {

			const string& str;
			size_t i;
			string& err;
			bool failed;
			bool allowComments;
			ParseOptions::DuplicateKeyPolicy duplicateKeyPolicy;
			std::shared_ptr<detail::StringArena> arena;

			string make_error(string&& msg) const {
				size_t line = 1, col = 1;
				for (size_t j = 0; j < i && j < str.size(); ++j) {
					if (str[j] == '\n') { line++; col = 1; }
					else { col++; }
				}
				return "line " + std::to_string(line) + ", col " + std::to_string(col) + ": " + msg;
			}

			Json fail(string&& msg) {
				string fullMsg = make_error(std::move(msg));
				if (!failed)
					err = fullMsg;
				failed = true;
				Json errNode(Type::Error);
				errNode.name = std::move(fullMsg);
				return errNode;
			}

			template <typename T> T fail(string&& msg, const T err_ret) {
				if (!failed)
					err = make_error(std::move(msg));
				failed = true;
				return err_ret;
			}

			void consume_whitespace() {
				while (i < str.size() && (str[i] == ' ' || str[i] == '\r' || str[i] == '\n' || str[i] == '\t'))
					i++;
			}

			bool consume_comment() {
				bool comment_found = false;
				if (i < str.size() && str[i] == '/') {
					if (!allowComments)
						return fail("comments are not allowed in strict mode", false);
					i++;
					if (i == str.size())
						return fail("unexpected end of input after start of comment", false);
					if (str[i] == '/') {
						i++;
						while (i < str.size() && str[i] != '\n') {
							i++;
						}
						comment_found = true;
					}
					else if (str[i] == '*') {
						i++;
						if (i > str.size() - 2)
							return fail("unexpected end of input inside multi-line comment", false);
						while (!(str[i] == '*' && str[i + 1] == '/')) {
							i++;
							if (i > str.size() - 2)
								return fail(
									"unexpected end of input inside multi-line comment", false);
						}
						i += 2;
						comment_found = true;
					}
					else
						return fail("malformed comment", false);
				}
				return comment_found;
			}

			void consume_garbage() {
				consume_whitespace();
				bool comment_found = false;
				do {
					comment_found = consume_comment();
					if (failed) return;
					consume_whitespace();
				} while (comment_found);
			}

			void encode_utf8(long pt, string& out) {
				if (pt < 0)
					return;

				if (pt < 0x80) {
					out += static_cast<char>(pt);
				}
				else if (pt < 0x800) {
					out += static_cast<char>((pt >> 6) | 0xC0);
					out += static_cast<char>((pt & 0x3F) | 0x80);
				}
				else if (pt < 0x10000) {
					out += static_cast<char>((pt >> 12) | 0xE0);
					out += static_cast<char>(((pt >> 6) & 0x3F) | 0x80);
					out += static_cast<char>((pt & 0x3F) | 0x80);
				}
				else {
					out += static_cast<char>((pt >> 18) | 0xF0);
					out += static_cast<char>(((pt >> 12) & 0x3F) | 0x80);
					out += static_cast<char>(((pt >> 6) & 0x3F) | 0x80);
					out += static_cast<char>((pt & 0x3F) | 0x80);
				}
			}

			// Stores an integer literal exactly.  Returns false when the text does not
			// fit int64 (signed) or uint64 (non-negative) either, which leaves the
			// caller to fall back to the double model.  std::from_chars is used rather
			// than atoi/strtoll so the result cannot depend on LC_NUMERIC.
			static bool storeIntegerLiteral(Json& rs, string_view token, bool negative) {
				if (negative) {
					int64_t value = 0;
					auto res = std::from_chars(token.data(), token.data() + token.size(), value);
					if (res.ec == std::errc{} && res.ptr == token.data() + token.size()) {
						rs.setNumberInt64(value);
						return true;
					}
					return false;
				}

				// Non-negative literals prefer the signed state - that way `1` from the
				// parser and `1` from an `int` are the same kind of node - and only a
				// value above INT64_MAX moves to the unsigned one.
				int64_t signedValue = 0;
				auto signedRes = std::from_chars(token.data(), token.data() + token.size(), signedValue);
				if (signedRes.ec == std::errc{} && signedRes.ptr == token.data() + token.size()) {
					rs.setNumberInt64(signedValue);
					return true;
				}

				uint64_t unsignedValue = 0;
				auto unsignedRes = std::from_chars(token.data(), token.data() + token.size(), unsignedValue);
				if (unsignedRes.ec == std::errc{} && unsignedRes.ptr == token.data() + token.size()) {
					rs.setNumberUint64(unsignedValue);
					return true;
				}
				return false;
			}

			Json parse_number() {
				Json rs(Type::Number);
				size_t start_pos = i;
				bool negative = false;

				if (i < str.size() && str[i] == '-') {
					negative = true;
					i++;
				}

				if (i == str.size())
					return fail("unexpected end of input in number");

				if (str[i] == '0') {
					i++;
					if (i < str.size() && in_range(str[i], '0', '9'))
						return fail("leading 0s not permitted in numbers");
				}
				else if (in_range(str[i], '1', '9')) {
					i++;
					while (i < str.size() && in_range(str[i], '0', '9'))
						i++;
				}
				else {
					return fail("invalid " + esc(str[i]) + " in number");
				}

				// R5-1: an integer literal - digits only, no fraction and no exponent -
				// is stored exactly, in whichever of the two integer states can hold it.
				// This is the whole point of the three-state model: `9007199254740993`
				// used to be rounded to a double here and printed back as `...992`.
				const bool literalEnds = (i == str.size()) || (str[i] != '.' && str[i] != 'e' && str[i] != 'E');
				if (literalEnds) {
					const string_view token(str.data() + start_pos, i - start_pos);
					if (negative && token.size() == 2 && token[1] == '0') {
						// "-0": the only integer spelling whose sign an integer state
						// cannot carry, so it stays a double and keeps printing as -0.
						// (The grammar rejects "-00", so length 2 plus that digit is
						// exactly the negative zero.)
						rs.setNumberDouble(detail::parseDouble(token));
						return rs;
					}
					if (storeIntegerLiteral(rs, token, negative))
						return rs;
					// Too wide for int64 and uint64: accepted, and degraded to double
					// exactly as before.
					rs.setNumberDouble(detail::parseDouble(token));
					return rs;
				}

				if (str[i] == '.') {
					i++;
					if (i == str.size() || !in_range(str[i], '0', '9'))
						return fail("at least one digit required in fractional part");

					while (i < str.size() && in_range(str[i], '0', '9'))
						i++;
				}

				if (i < str.size() && (str[i] == 'e' || str[i] == 'E')) {
					i++;

					if (i < str.size() && (str[i] == '+' || str[i] == '-'))
						i++;

					if (i == str.size() || !in_range(str[i], '0', '9'))
						return fail("at least one digit required in exponent");

					while (i < str.size() && in_range(str[i], '0', '9'))
						i++;
				}

				rs.setNumberDouble(detail::parseDouble(string_view(str.data() + start_pos, i - start_pos)));
				return rs;
			}

			string parse_string() {
				string out;
				long last_escaped_codepoint = -1;
				while (true) {
					if (i == str.size())
						return fail("unexpected end of input in string", "");

					char ch = str[i++];

					if (ch == '"') {
						encode_utf8(last_escaped_codepoint, out);
						return out;
					}

					if (in_range(ch, 0, 0x1f))
						return fail("unescaped " + esc(ch) + " in string", "");

					if (ch != '\\') {
						encode_utf8(last_escaped_codepoint, out);
						last_escaped_codepoint = -1;
						out += ch;
						continue;
					}

					if (i == str.size())
						return fail("unexpected end of input in string", "");

					ch = str[i++];

					if (ch == 'u') {
						string esc = str.substr(i, 4);
						if (esc.length() < 4) {
							return fail("bad \\u escape: " + esc, "");
						}
						for (size_t j = 0; j < 4; j++) {
							if (!in_range(esc[j], 'a', 'f') && !in_range(esc[j], 'A', 'F')
								&& !in_range(esc[j], '0', '9'))
								return fail("bad \\u escape: " + esc, "");
						}

						long codepoint = strtol(esc.data(), nullptr, 16);

						if (in_range(last_escaped_codepoint, 0xD800, 0xDBFF)
							&& in_range(codepoint, 0xDC00, 0xDFFF)) {
							encode_utf8((((last_escaped_codepoint - 0xD800) << 10)
								| (codepoint - 0xDC00)) + 0x10000, out);
							last_escaped_codepoint = -1;
						}
						else {
							encode_utf8(last_escaped_codepoint, out);
							last_escaped_codepoint = codepoint;
						}

						i += 4;
						continue;
					}

					encode_utf8(last_escaped_codepoint, out);
					last_escaped_codepoint = -1;

					if (ch == 'b') {
						out += '\b';
					}
					else if (ch == 'f') {
						out += '\f';
					}
					else if (ch == 'n') {
						out += '\n';
					}
					else if (ch == 'r') {
						out += '\r';
					}
					else if (ch == 't') {
						out += '\t';
					}
					else if (ch == '"' || ch == '\\' || ch == '/') {
						out += ch;
					}
					else {
						return fail("invalid escape character " + esc(ch), "");
					}
				}
			}

			detail::StoredString parse_stored_string() {
				size_t start = i;
				for (size_t pos = i; pos < str.size(); ++pos) {
					char ch = str[pos];
					if (ch == '"') {
						i = pos + 1;
						return detail::StoredString::fromView(arena, arena->view(start, pos - start));
					}
					if (ch == '\\' || in_range(ch, 0, 0x1f)) {
						i = start;
						string decoded = parse_string();
						if (failed)
							return detail::StoredString();
						return detail::StoredString::fromView(arena, arena->store(std::move(decoded)));
					}
				}
				i = start;
				string decoded = parse_string();
				if (failed)
					return detail::StoredString();
				return detail::StoredString::fromView(arena, arena->store(std::move(decoded)));
			}

			// --------------- Explicit-stack PDA parser ---------------
			// Replaces the former recursive-descent parse_json(depth).
			// Uses a heap-allocated stack instead of the call stack,
			// eliminating stack-overflow risk for deeply nested input.
			Json parse_json_pda() {
				enum class PState {
					VALUE,              // expect any JSON value
					OBJ_KEY_OR_END,     // inside object: expect '"' (key) or '}'
					OBJ_KEY,            // inside object after comma: expect '"' (key)
					OBJ_COLON,          // expect ':'
					OBJ_COMMA_OR_END,   // expect ',' or '}'
					ARR_COMMA_OR_END    // expect ',' or ']'
				};
				static constexpr size_t keyIndexThreshold = 16;
				struct Frame {
					Json* container;
					detail::StoredString key;
					// Up to keyIndexThreshold keys are compared by a linear scan of this inline
					// array, which needs no allocation at all - the common case for real
					// documents, whose objects are small.  Measured at 3.1 ns/key, so keeping it
					// is worth the 384 bytes per frame (2.8 ns per container push_back, ~1% of a
					// parse); adding a hash prefilter to it made it slower, not faster.
					std::array<std::pair<string_view, Json*>, keyIndexThreshold> smallKeyIndex;
					size_t smallKeyCount;
					// Objects wider than that switch to this flat hash index.  It replaced a
					// std::unordered_map<string_view, Json*> which cost 45-77 ns and one heap
					// node per key and dominated wide-object parsing; see detail::JsonKeyIndex.
					detail::JsonKeyIndex<Json> wideKeyIndex;
					bool useHashIndex;
					PState childDone;   // state to resume after delivering a child value
				};

				std::vector<Frame> stk;
				stk.reserve(32);

				// RAII guard: delete remaining heap containers on any exit path
				struct Cleanup {
					std::vector<Frame>& s;
					~Cleanup() { for (auto& f : s) delete f.container; }
				} guard{ stk };

				Json root(Type::Error);
				PState state = PState::VALUE;

				auto attachToParent = [&](Frame& parent, Json* node) -> bool {
					if (parent.container->type == Type::Object) {
						string_view keyView = parent.key.view();
						bool foundExisting = false;
						size_t smallIndex = 0;
						detail::JsonKeyIndex<Json>::Slot* wideSlot = nullptr;
						if (parent.useHashIndex) {
							wideSlot = parent.wideKeyIndex.probe(keyView, detail::JsonKeyIndex<Json>::hashKey(keyView));
							foundExisting = wideSlot != nullptr;
						} else {
							for (size_t index = 0; index < parent.smallKeyCount; ++index) {
								if (parent.smallKeyIndex[index].first == keyView) {
									foundExisting = true;
									smallIndex = index;
									break;
								}
							}
						}

						if (foundExisting) {
							switch (duplicateKeyPolicy) {
							case ParseOptions::DuplicateKeyPolicy::KeepFirst:
								parent.container->deleteJson(node);
								state = parent.childDone;
								parent.key.clear();
								return true;
							case ParseOptions::DuplicateKeyPolicy::KeepLast:
								parent.container->removeDirectChildrenByKey(keyView);
								break;
							case ParseOptions::DuplicateKeyPolicy::Reject:
								parent.container->deleteJson(node);
								{
									const string key = parent.key;
									fail("duplicate key '" + key + "' in object");
								}
								return false;
							}
						}
						node->name = std::move(parent.key);
						if (parent.useHashIndex) {
							// The key text has not changed, so the stored hash still describes it -
							// only the node moves on to the replacement member.
							if (wideSlot)
								wideSlot->node = node;
							else
								parent.wideKeyIndex.assign(node->name.view(), node);
						} else if (foundExisting) {
							parent.smallKeyIndex[smallIndex] = { node->name.view(), node };
						} else if (parent.smallKeyCount < keyIndexThreshold) {
							parent.smallKeyIndex[parent.smallKeyCount++] = { node->name.view(), node };
						} else {
							// The 17th key: move the inline entries into the hash index and
							// continue there.
							parent.useHashIndex = true;
							parent.wideKeyIndex.reserve(keyIndexThreshold + 1);
							for (size_t index = 0; index < parent.smallKeyCount; ++index)
								parent.wideKeyIndex.assign(parent.smallKeyIndex[index].first,
									                       parent.smallKeyIndex[index].second);
							parent.wideKeyIndex.assign(node->name.view(), node);
							parent.smallKeyCount = 0;
						}
					}
					parent.container->appendNodeToJson(node);
					state = parent.childDone;
					return true;
				};

				// Close the top-most container and deliver to parent or set root
				auto closeTop = [&]() -> bool {
					Json* done = stk.back().container;
					stk.back().container = nullptr;
					stk.pop_back();
					if (stk.empty()) {
						root = std::move(*done);
						delete done;
						return true;
					}
					Frame& parent = stk.back();
					if (!attachToParent(parent, done))
						return true;
					return false;
				};

				// Deliver a parsed leaf value to the current container or set root
				auto deliver = [&](Json* val) -> bool {
					if (stk.empty()) {
						root = std::move(*val);
						delete val;
						return true;
					}
					Frame& top = stk.back();
					if (!attachToParent(top, val))
						return true;
					return false;
				};

				for (;;) {
					consume_garbage();
					if (failed) return Json(Type::Error);
					if (i >= str.size()) return fail("unexpected end of input");

					char ch = str[i];

					switch (state) {

					case PState::VALUE: {
						// ---- open object ----
						if (ch == '{') {
							i++;
							if (stk.size() > static_cast<size_t>(max_depth))
								return fail("exceeded maximum nesting depth");
							stk.push_back(Frame{ new Json(Type::Object), detail::StoredString(), {}, 0, {}, false, PState::OBJ_COMMA_OR_END });
							state = PState::OBJ_KEY_OR_END;
							break;
						}
						// ---- open array ----
						if (ch == '[') {
							i++;
							if (stk.size() > static_cast<size_t>(max_depth))
								return fail("exceeded maximum nesting depth");
							stk.push_back(Frame{ new Json(Type::Array), detail::StoredString(), {}, 0, {}, false, PState::ARR_COMMA_OR_END });
							consume_garbage();
							if (failed) return Json(Type::Error);
							if (i < str.size() && str[i] == ']') {
								i++;
								if (closeTop()) return root;
								break;
							}
							state = PState::VALUE;
							break;
						}
						// ---- primitives ----
						Json* val = nullptr;
						if (ch == '"') {
							i++;
							detail::StoredString s = parse_stored_string();
							if (failed) return Json(Type::Error);
							val = new Json(Type::String);
							val->valueString = std::move(s);
						}
						else if (ch == '-' || (ch >= '0' && ch <= '9')) {
							Json num = parse_number();
							if (failed) return Json(Type::Error);
							val = new Json(std::move(num));
						}
						else if (ch == 't') {
							size_t rem = str.size() - i;
							if (rem >= 4 && str.compare(i, 4, "true") == 0) {
								i += 4; val = new Json(Type::True);
							} else {
								return fail("parse error: expected true, got " + str.substr(i, std::min(rem, static_cast<size_t>(4))));
							}
						}
						else if (ch == 'f') {
							size_t rem = str.size() - i;
							if (rem >= 5 && str.compare(i, 5, "false") == 0) {
								i += 5; val = new Json(Type::False);
							} else {
								return fail("parse error: expected false, got " + str.substr(i, std::min(rem, static_cast<size_t>(5))));
							}
						}
						else if (ch == 'n') {
							size_t rem = str.size() - i;
							if (rem >= 4 && str.compare(i, 4, "null") == 0) {
								i += 4; val = new Json(Type::Null);
							} else {
								return fail("parse error: expected null, got " + str.substr(i, std::min(rem, static_cast<size_t>(4))));
							}
						}
						else {
							return fail("expected value, got " + esc(ch));
						}
						if (deliver(val)) return root;
						break;
					}

					case PState::OBJ_KEY_OR_END: {
						if (ch == '}') {
							i++;
							if (closeTop()) return root;
							break;
						}
						if (ch != '"')
							return fail("expected '\"' in object, got " + esc(ch));
						i++;
						stk.back().key = parse_stored_string();
						if (failed) return Json(Type::Error);
						state = PState::OBJ_COLON;
						break;
					}

					case PState::OBJ_KEY: {
						if (ch != '"')
							return fail("expected '\"' in object, got " + esc(ch));
						i++;
						stk.back().key = parse_stored_string();
						if (failed) return Json(Type::Error);
						state = PState::OBJ_COLON;
						break;
					}

					case PState::OBJ_COLON: {
						if (ch != ':')
							return fail("expected ':' in object, got " + esc(ch));
						i++;
						state = PState::VALUE;
						break;
					}

					case PState::OBJ_COMMA_OR_END: {
						if (ch == '}') {
							i++;
							if (closeTop()) return root;
							break;
						}
						if (ch != ',')
							return fail("expected ',' in object, got " + esc(ch));
						i++;
						state = PState::OBJ_KEY;
						break;
					}

					case PState::ARR_COMMA_OR_END: {
						if (ch == ']') {
							i++;
							if (closeTop()) return root;
							break;
						}
						if (ch != ',')
							return fail("expected ',' in array, got " + esc(ch));
						i++;
						state = PState::VALUE;
						break;
					}

					} // switch
				} // for
			}

		};

	};

	class JsonEntry {
		const detail::StoredString* keyPtr;
		Json* valuePtr;
	public:
		JsonEntry() : keyPtr(nullptr), valuePtr(nullptr) {}
		void reset(Json* ptr) {
			keyPtr = ptr ? &ptr->name : nullptr;
			valuePtr = ptr;
		}
		const string& key() const {
			return keyPtr->strRef();
		}
		Json& value() const {
			return *valuePtr;
		}
	};

	class JsonConstEntry {
		const detail::StoredString* keyPtr;
		const Json* valuePtr;
	public:
		JsonConstEntry() : keyPtr(nullptr), valuePtr(nullptr) {}
		void reset(const Json* ptr) {
			keyPtr = ptr ? &ptr->name : nullptr;
			valuePtr = ptr;
		}
		const string& key() const {
			return keyPtr->strRef();
		}
		const Json& value() const {
			return *valuePtr;
		}
	};

	class JsonIterator
	{
		Json* ptr;
		mutable JsonEntry entry;
	public:
		using iterator_category = std::forward_iterator_tag;
		using value_type = JsonEntry;
		using difference_type = std::ptrdiff_t;
		using pointer = JsonEntry*;
		using reference = JsonEntry&;

		explicit JsonIterator(const Json& p) {
			ptr = p.child;
			entry.reset(ptr);
		}
		explicit JsonIterator(Json* p) {
			ptr = p;
			entry.reset(ptr);
		}
		JsonEntry& operator*() const {
			entry.reset(ptr);
			return entry;
		}
		JsonEntry* operator->() const {
			entry.reset(ptr);
			return &entry;
		}
		JsonIterator& operator++() {
			if (ptr)
				ptr = ptr->brother;
			entry.reset(ptr);
			return *this;
		}
		JsonIterator operator++(int) {
			JsonIterator copy(*this);
			++(*this);
			return copy;
		}
		bool operator!=(JsonIterator const& other) const
		{
			return this->ptr != other.ptr;
		}
		bool operator==(const JsonIterator& other) const
		{
			return this->ptr == other.ptr;
		}

		JsonIterator& begin() {
			return *this;
		}

		JsonIterator end() {
			return JsonIterator(nullptr);
		}

		string key() const {
			return ptr ? ptr->name.str() : string();
		}

		Json& value() const {
			return *ptr;
		}
	};

	class JsonConstIterator
	{
		const Json* ptr;
		mutable JsonConstEntry entry;
	public:
		using iterator_category = std::forward_iterator_tag;
		using value_type = JsonConstEntry;
		using difference_type = std::ptrdiff_t;
		using pointer = JsonConstEntry*;
		using reference = JsonConstEntry&;

		explicit JsonConstIterator(const Json& p) {
			ptr = p.child;
			entry.reset(ptr);
		}
		explicit JsonConstIterator(const Json* p) {
			ptr = p;
			entry.reset(ptr);
		}
		JsonConstEntry& operator*() const {
			entry.reset(ptr);
			return entry;
		}
		JsonConstEntry* operator->() const {
			entry.reset(ptr);
			return &entry;
		}
		JsonConstIterator& operator++() {
			if (ptr)
				ptr = ptr->brother;
			entry.reset(ptr);
			return *this;
		}
		JsonConstIterator operator++(int) {
			JsonConstIterator copy(*this);
			++(*this);
			return copy;
		}
		bool operator!=(const JsonConstIterator& other) const {
			return this->ptr != other.ptr;
		}
		bool operator==(const JsonConstIterator& other) const {
			return this->ptr == other.ptr;
		}
		JsonConstIterator begin() const {
			return *this;
		}
		JsonConstIterator end() const {
			return JsonConstIterator(nullptr);
		}
		string key() const {
			return ptr ? ptr->name.str() : string();
		}
		const Json& value() const {
			return *ptr;
		}
	};

	template <size_t I>
	decltype(auto) get(JsonEntry& entry) {
		static_assert(I < 2, "JsonEntry index out of bounds");
		if constexpr (I == 0) return (entry.key());
		else return (entry.value());
	}

	template <size_t I>
	decltype(auto) get(const JsonEntry& entry) {
		static_assert(I < 2, "JsonEntry index out of bounds");
		if constexpr (I == 0) return (entry.key());
		else return (entry.value());
	}

	template <size_t I>
	decltype(auto) get(JsonConstEntry& entry) {
		static_assert(I < 2, "JsonConstEntry index out of bounds");
		if constexpr (I == 0) return (entry.key());
		else return (entry.value());
	}

	template <size_t I>
	decltype(auto) get(const JsonConstEntry& entry) {
		static_assert(I < 2, "JsonConstEntry index out of bounds");
		if constexpr (I == 0) return (entry.key());
		else return (entry.value());
	}

	inline Json::iterator Json::begin() {
		return JsonIterator(this->child);
	}

	inline Json::iterator Json::end() {
		return JsonIterator(nullptr);
	}

	inline Json::const_iterator Json::begin() const {
		return JsonConstIterator(this->child);
	}

	inline Json::const_iterator Json::end() const {
		return JsonConstIterator(nullptr);
	}

	inline Json::const_iterator Json::cbegin() const {
		return JsonConstIterator(this->child);
	}

	inline Json::const_iterator Json::cend() const {
		return JsonConstIterator(nullptr);
	}

}

namespace std {
	template <>
	struct tuple_size<ZJSON::JsonEntry> : integral_constant<size_t, 2> {};

	template <>
	struct tuple_element<0, ZJSON::JsonEntry> {
		using type = const ZJSON::string;
	};

	template <>
	struct tuple_element<1, ZJSON::JsonEntry> {
		using type = ZJSON::Json;
	};

	template <>
	struct tuple_size<ZJSON::JsonConstEntry> : integral_constant<size_t, 2> {};

	template <>
	struct tuple_element<0, ZJSON::JsonConstEntry> {
		using type = const ZJSON::string;
	};

	template <>
	struct tuple_element<1, ZJSON::JsonConstEntry> {
		using type = const ZJSON::Json;
	};

	// Note on element access.
	//
	// The tuple protocol above (tuple_size + tuple_element specialisations, which the
	// standard does allow) plus the ADL-findable ZJSON::get are what structured bindings
	// use, so `for (auto& [key, value] : json)` works.  A `std::get<N>(entry)` call is
	// deliberately NOT provided: std::get's primary templates are declared for
	// std::pair/std::tuple, so no valid specialisation exists for our own types, and
	// merely adding overloads to namespace std would be undefined behaviour.  Write
	// `using std::get; get<0>(entry);` when a named form is wanted.
}