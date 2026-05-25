#pragma once
#pragma execution_character_set("utf-8")

#include <string>
#include <vector>
#include <stack>
#include <iostream>
#include <algorithm>
#include <limits.h>
#include <array>
#include <cstring>
#include <cmath>
#include <fstream>
#include <cstdio>
#include <iterator>
#include <unordered_map>
#include <charconv>
#include <system_error>
#include <type_traits>

namespace ZJSON {
	using std::string;
	using std::move;
	using std::vector;
	using std::stack;

	static const int max_depth = 100;
	static const std::array<string, 8> TYPENAMES{ "Error", "False", "True", "Null", "Number", "String", "Object", "Array" };

	static inline bool stringEndWith(const string& str, const string& tail) {
		if (str.size() < tail.size()) return false;
		return str.compare(str.size() - tail.size(), tail.size(), tail) == 0;
	}

	enum class JsonType
	{
		Object = 6,
		Array = 7
	};

	// ----------------------------------------------------------------------
	// Number → string serialization (RFC 8259 compliant).
	// Uses std::to_chars (C++17, shortest round-trip) when the implementation
	// provides the floating-point overload; otherwise falls back to a
	// locale-independent snprintf with %.17g (which is round-trip safe for
	// IEEE-754 double precision).
	// ----------------------------------------------------------------------
	namespace detail {
		template <typename T, typename = void>
		struct has_fp_to_chars : std::false_type {};

		template <typename T>
		struct has_fp_to_chars<T, std::void_t<decltype(std::to_chars(
			std::declval<char*>(), std::declval<char*>(), std::declval<T>()))>>
			: std::true_type {};

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

	// Append a JSON-formatted number. NaN/Inf are written as "null" because
	// RFC 8259 forbids them; callers that wish to reject earlier may do so.
	static inline void appendNumber(double v, string& out) {
		if (!std::isfinite(v)) {
			out.append("null");
			return;
		}
		// Integer fast-path: exact integer that fits in long long → write %lld.
		// Preserves a leading '-' for negative zero (e.g. -0.0 → "-0").
		if (v == std::floor(v) && v >= -1e16 && v <= 1e16) {
			long long i = static_cast<long long>(v);
			if (static_cast<double>(i) == v) {
				char buf[32];
				if (i == 0 && std::signbit(v)) {
					out.append("-0");
				} else {
					int n = std::snprintf(buf, sizeof(buf), "%lld", i);
					if (n > 0) out.append(buf, static_cast<size_t>(n));
				}
				return;
			}
		}
		detail::appendDoubleImpl(v, out, detail::has_fp_to_chars<double>{});
	}

	static inline void appendEscapedString(const string& input, string& out) {
		for (unsigned char ch : input) {
			switch (ch) {
			case '"': out.append("\\\""); break;
			case '\\': out.append("\\\\"); break;
			case '\b': out.append("\\b"); break;
			case '\f': out.append("\\f"); break;
			case '\n': out.append("\\n"); break;
			case '\r': out.append("\\r"); break;
			case '\t': out.append("\\t"); break;
			default:
				if (ch < 0x20) {
					char buf[7];
					snprintf(buf, sizeof(buf), "\\u%04x", ch);
					out.append(buf);
				}
				else {
					out.push_back(static_cast<char>(ch));
				}
			}
		}
	}

	static inline void appendQuotedKey(const string& key, string& out) {
		out.push_back('"');
		appendEscapedString(key, out);
		out.append("\":");
	}

	// UTF-8 byte sequence validator (RFC 3629)
	static inline bool validate_utf8_bytes(const string& s, size_t& errorPos) {
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

	class Json final {
		friend class JsonIterator;
	private:
		Json* brother;
		Json* child;
		Json* lastChild;   // tail of child list — O(1) append
		mutable std::unordered_map<string, Json*>* keymap;  // lazy O(1) key lookup (Object only)
		Type type;
		string valueString;
		double valueNumber;
		string name;

		static Json parse(const std::string& in, std::string& err, bool allowComments = true, bool validateUtf8 = false)
		{
			if (validateUtf8) {
				size_t errPos = 0;
				if (!validate_utf8_bytes(in, errPos)) {
					err = "invalid UTF-8 byte at position " + std::to_string(errPos);
					return Json(Type::Error);
				}
			}
			JsonParser parser{ in, 0, err, false, allowComments };
			Json result = parser.parse_json_pda();
			if (result.type == Type::Error)
				return result;
			parser.consume_garbage();
			if (parser.i != in.size())
				return parser.fail("unexpected trailing " + esc(in[parser.i]));
			return result;
		}

		static Json parse(const char* in, std::string& err, bool allowComments = true, bool validateUtf8 = false) {
			if (in) {
				return parse(std::string(in), err, allowComments, validateUtf8);
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
			this->valueNumber = 0;
		}

		// Clones source's sibling chain; *outTail receives the last node (may be nullptr if source is nullptr)
		static Json* cloneChain(const Json* source, Json** outTail = nullptr) {
			if (!source) {
				if (outTail) *outTail = nullptr;
				return nullptr;
			}
			Json* head = new Json(source->type);
			head->name = source->name;
			head->valueString = source->valueString;
			head->valueNumber = source->valueNumber;
			Json* childTail = nullptr;
			head->child = cloneChain(source->child, &childTail);
			head->lastChild = childTail;
			Json* tail = head;
			const Json* cur = source->brother;
			while (cur) {
				Json* node = new Json(cur->type);
				node->name = cur->name;
				node->valueString = cur->valueString;
				node->valueNumber = cur->valueNumber;
				Json* nodeChildTail = nullptr;
				node->child = cloneChain(cur->child, &nodeChildTail);
				node->lastChild = nodeChildTail;
				tail->brother = node;
				tail = node;
				cur = cur->brother;
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
		Json(JsonType type = JsonType::Object) {
			this->brother = nullptr;
			this->child = nullptr;
			this->lastChild = nullptr;
			this->keymap = nullptr;
			this->type = (Type)type;
		}

		template<typename T> Json(const T& value) {
			this->brother = nullptr;
			this->child = nullptr;
			this->lastChild = nullptr;
			this->keymap = nullptr;
			this->valueNumber = value;
			this->type = Type::Number;
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
				this->valueNumber = value;
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
				this->valueNumber = value;
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
			this->valueNumber = origin.valueNumber;
			Json* childTail = nullptr;
			this->child = cloneChain(origin.child, &childTail);
			this->lastChild = childTail;
		}

		Json(Json&& rhs) noexcept {
			this->type = rhs.type;
			this->child = rhs.child;
			this->brother = rhs.brother;
			this->lastChild = rhs.lastChild;
			this->keymap = rhs.keymap;
			this->name = std::move(rhs.name);
			this->valueString = std::move(rhs.valueString);
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

		static Json FromFile(const char* filepath) {
			if (!filepath)
				return Json(Type::Error);
			std::ifstream file(filepath, std::ios::binary);
			if (!file.is_open())
				return Json(Type::Error);
			std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
			if (content.empty())
				return Json(Type::Error);
			return Json(content);
		}

		static Json FromFile(const std::string& filepath) {
			return Json::FromFile(filepath.c_str());
		}

		static Json ParseJson(const std::string& input, std::string& errMsg) {
			return parse(input, errMsg);
		}

		static Json ParseJsonStrict(const std::string& input, std::string& errMsg) {
			return parse(input, errMsg, false);
		}

		static Json ParseJsonStrictUtf8(const std::string& input, std::string& errMsg) {
			return parse(input, errMsg, false, true);
		}

		~Json() {
			releaseChildren();
		}

		Json& operator = (const Json& origin) {
			if (this == &origin)
				return(*this);
			releaseChildren();
			this->brother = nullptr;
			this->type = origin.type;
			this->name = origin.name;
			this->valueString = origin.valueString;
			this->valueNumber = origin.valueNumber;
			Json* childTail = nullptr;
			this->child = cloneChain(origin.child, &childTail);
			this->lastChild = childTail;
			return(*this);
		}

		Json& operator = (Json&& rhs) noexcept {
			if (this == &rhs)
				return(*this);
			releaseChildren();
			this->type = rhs.type;
			this->child = rhs.child;
			this->brother = rhs.brother;
			this->lastChild = rhs.lastChild;
			this->keymap = rhs.keymap;
			this->name = std::move(rhs.name);
			this->valueString = std::move(rhs.valueString);
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

		Json operator[](const string& key) const {
			if (this->type != Type::Object || key.empty() || !this->child)
				return Json(Type::Error);
			// Fast path: keymap lookup for O(1) direct-child access
			if (!this->keymap) buildKeymap();
			auto it = this->keymap->find(key);
			if (it != this->keymap->end()) return *(it->second);
			// Slow path: deep search for nested keys (uncommon)
			return this->child->find(key);
		}

		bool contains(const string& key) const {
			if (this->type != Type::Object || key.empty() || !this->child) return false;
			if (!this->keymap) buildKeymap();
			return this->keymap->count(key) > 0;
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

		Json takes(int start, int end = 0) {
			Json rs(Type::Array);
			if (this->type == Type::Array) {
				if (end == 0)
					end = this->size();
				while (start < end) {
					rs.push_back(this->take(start));
					end--;
				}
			}
			return rs;
		}

		Json slice(int start, int end = 0) const {
			Json rs(Type::Array);
			if (this->type == Type::Array) {
				if (end == 0)
					end = this->size();
				while (start < end)
					rs.push_back((*this)[start++]);
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
					if (cur->type != Type::Error && cur->name.length() > 0)
						rs.push_back(cur->name);
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
				return add("", value);
			else
				return (*this);
		}

		Json& add(const Json& value) {
			if (this->type == Type::Array)
				return add("", value);
			else
				return (*this);
		}

		Json& add(Json&& value) {
			return add("", std::move(value));
		}

		template<typename T> Json& add(string name, T value) {
			if ((!name.empty() && this->type == Type::Object) || this->type == Type::Array) {
				Json* node = new Json(value);
				node->name = this->type == Type::Object ? name : "";
				appendNodeToJson(node);
			}
			return (*this);
		}

		Json& add(string name, const Json& value) {
			if ((!name.empty() && this->type == Type::Object) || this->type == Type::Array) {
				Json* node = new Json(value);
				node->name = this->type == Type::Object ? name : "";
				appendNodeToJson(node);
			}
			return (*this);
		}

		Json& add(string name, Json&& value) {
			if ((!name.empty() && this->type == Type::Object) || this->type == Type::Array) {
				Json* node = new Json(std::move(value));
				node->name = this->type == Type::Object ? name : "";
				appendNodeToJson(node);
			}
			return (*this);
		}

		bool isError() const {
			return this->type == Type::Error;
		}

		bool isNull() const {
			return this->type == Type::Null;
		}

		bool isObject() const {
			return this->type == Type::Object;
		}

		bool isArray() const {
			return this->type == Type::Array;
		}

		bool isNumber() const {
			return this->type == Type::Number;
		}

		bool isTrue() const {
			return this->type == Type::True;
		}

		bool isFalse() const {
			return this->type == Type::False;
		}

		bool isString() const {
			return this->type == Type::String;
		}

		int size() const {
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

		bool isEmpty() const {
			return this->size() <= 0;
		}

		string toString() const {
			if (this->type == Type::Error) {
				return "";
			}
			if (this->type == Type::String) {
				return this->valueString;
			}
			else {
				string result;
				if (this->type == Type::Object || this->type == Type::Array) {
					if (this->child)
						this->toString(this, result);		//this->toString(this, result, 0, this->type == Type::Object); 
					else
						return this->type == Type::Object ? "{}" : "[]";
				}
				else
					this->valueJsonToString(this, result, false);
				return stringEndWith(result, ",") ? result.erase(result.length() - 1) : result;
			}
		}

		int toInt() const {
			return (int)this->toDouble();
		}

		double toDouble() const {
			if (this->type == Type::Number)
				return valueNumber;
			else if (this->isTrue())
				return 1;
			else if (this->isFalse())
				return 0;
			else if (this->type == Type::String) {
				return atof(this->toString().c_str());
			}
			else
				return 0;
		}

		float toFloat() const {
			return (float)this->toDouble();
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
					this->remove(cur->name);
					this->extendItem(cur);
					cur = cur->brother;
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
						this->extendItem(cur);
						cur = cur->brother;
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
			if (this->type == Type::Array) {
				if (index < 0) {
					index += this->size();
					if (index < 0)
						return (*this);
				}
				if (index == 0)
					return this->push_front(value);
				else {
					int ct = 0;
					Json* pre = this;
					Json* cur = this->child;
					while (cur) {
						if (index == ct++)
							break;
						pre = cur;
						cur = cur->brother;
					}
					if (index < ct) {
						pre->brother = new Json(value);
						pre->brother->brother = cur;
						if (!cur) this->lastChild = pre->brother; // inserted at tail
					}
					return (*this);
				}
			}
			else
				return (*this);
		}
		Json& insert(int index, Json&& value) {
			if (this->type == Type::Array) {
				if (index < 0) {
					index += this->size();
					if (index < 0)
						return (*this);
				}
				if (index == 0)
					return this->push_front(std::move(value));
				else {
					int ct = 0;
					Json* pre = this;
					Json* cur = this->child;
					while (cur) {
						if (index == ct++)
							break;
						pre = cur;
						cur = cur->brother;
					}
					if (index < ct) {
						pre->brother = new Json(std::move(value));
						pre->brother->brother = cur;
						if (!cur) this->lastChild = pre->brother; // inserted at tail
					}
					return (*this);
				}
			}
			else
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

		Json& remove(const string& key, Json* self = nullptr, Json* prev = nullptr)
		{
			if (key.empty() || (self == nullptr && this->type != Type::Object))
				return (*this);
			if (self == nullptr) {
				// Top-level call: invalidate keymap and lastChild since chain will change
				if (this->keymap) { delete this->keymap; this->keymap = nullptr; }
				this->lastChild = nullptr;
				self = this;
			}
			Json* cur = self;
			Json* pre = self;
			if (prev)
				pre = prev;
			bool found = false;
			do
			{
				if (cur->name == key)
				{
					if (pre->type == Type::Array || pre->type == Type::Object) {
						pre->child = cur->brother;
					}
					else if (cur->type == Type::Array || cur->type == Type::Object)
						pre->brother = cur->brother;
					else
						pre->brother = cur->brother;
					found = true;
				}
				else if (cur->type == Type::Object || cur->type == Type::Array)
				{
					if (cur->child)
					{
						remove(key, cur->child, cur);
					}
				}
				if (found)
				{
					auto tmp = pre->brother ? pre->brother->brother : (cur->brother ? cur->brother : nullptr);
					if (cur->child)
						deleteJson(cur->child);
					cur->child = nullptr;
					cur->brother = nullptr;
					delete cur;
					cur = tmp;
					//pre->brother = cur;
					found = false;
				}
				else
				{
					pre = cur;
					cur = cur->brother;
				}

			} while (cur);
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
			if (this->type == Type::Array) {
				int ct = 0;
				Json* cur = this->child;
				Json rs(Type::Error);
				while (cur && cur->toString().compare(value) != 0)
				{
					cur = cur->brother;
					ct++;
				}
				return this->size() <= ct ? -1 : ct;
			}
			else
				return -1;
		}

	private:
		void extendItem(Json* cur) {
			switch (cur->type)
			{
			case Type::False:
				this->add(cur->name, false);
				break;
			case Type::True:
				this->add(cur->name, true);
				break;
			case Type::Null:
				this->add(cur->name, nullptr);
				break;
			case Type::Number:
				this->add(cur->name, cur->valueNumber);
				break;
			case Type::String:
				this->add(cur->name, cur->valueString);
				break;
			case Type::Object:
			case Type::Array:
				this->add(cur->name, *cur);
			default:
				;
			}
		}

		void appendNodeToJson(Json* node, Json* self = nullptr)
		{
			if (self == nullptr)
				self = this;
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
			// Invalidate keymap so it will be lazily rebuilt on next operator[]
			if (self->keymap) { delete self->keymap; self->keymap = nullptr; }
		}

		void deleteJson(Json* obj) {		//all json type is value type
			if (!obj) return;
			Json* cur = obj;
			Json* follow = obj;
			do {
				cur = follow;
				follow = follow->brother;
				if (cur->type == Type::Object || cur->type == Type::Array) {
					if (cur->child) {
						deleteJson(cur->child);
						cur->child = nullptr;
					}
				}
				if (cur->keymap) { delete cur->keymap; cur->keymap = nullptr; }
				delete cur;
				cur = nullptr;
			} while (follow);
		}

		// Lazily build a keymap of all immediate children (Object keys only)
		void buildKeymap() const {
			if (keymap) { delete keymap; keymap = nullptr; }
			keymap = new std::unordered_map<string, Json*>();
			keymap->reserve(16);
			Json* cur = child;
			while (cur) {
				if (!cur->name.empty())
					keymap->emplace(cur->name, cur);
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

		Json find(const string& key, bool notArray = true) {
			if (this->type == Type::Array || this->type == Type::Object) {
				if (this->brother) {
					if (this->brother->name == key)
						return *(this->brother);
					else {
						Json rs = this->brother->find(key, this->brother->type != Type::Array);
						if (!rs.isError())
							return rs;
					}
				}
				if (this->child) {
					if (this->child->name == key)
						return *(this->child);
					else {
						Json rs = this->child->find(key, this->type != Type::Array);
						if (!rs.isError())
							return rs;
					}
				}
				return Json(Type::Error);
			}
			else {
				Json* cur = this;
				Json rs(Type::Error);
				while (cur)
				{
					if (notArray && cur->name == key) {
						return Json(*cur);
					}
					else {
						if (cur->type == Type::Array || cur->type == Type::Object) {
							if (cur->child) {
								rs = cur->find(key, cur->type != Type::Array);
								if (rs.type != Type::Error)
									break;
							}
						}
						cur = cur->brother;
					}
				}
				return rs;
			}
		}

		void valueJsonToString(const Json* json, string& result, bool isObj = true) const {
			if (json->type == Type::String) {
				if (isObj)
					appendQuotedKey(json->name, result);
				result.push_back('"');
				appendEscapedString(json->valueString, result);
				result.append("\",");
			}
			else if (json->type == Type::Number) {
				if (isObj)
					appendQuotedKey(json->name, result);
				appendNumber(json->valueNumber, result);
				result.append(",");
			}
			else if (json->type == Type::True) {
				if (isObj)
					appendQuotedKey(json->name, result);
				result.append("true,");
			}
			else if (json->type == Type::False) {
				if (isObj)
					appendQuotedKey(json->name, result);
				result.append("false,");
			}
			else if (json->type == Type::Null) {
				if (isObj)
					appendQuotedKey(json->name, result);
				result.append("null,");
			}
		}

		void toString(const Json* json, string& result) const {
			stack<Json*> s;
			bool flag = true;
			Json* cur = (Json*)json;
			if (cur) {
				result.append(cur->type == Type::Object ? "{" : "[");
				s.push(cur);
			}
			else
				return;
			cur = cur->child;
			while (cur || !s.empty()) {
				if (cur) {
					if (cur->type == Type::Object || cur->type == Type::Array) {
						if (!cur->name.empty())
							appendQuotedKey(cur->name, result);
						result.append(cur->type == Type::Object ? "{" : "[");
						if (cur->child == nullptr)
							result.append(cur->type == Type::Object ? "}," : "],");
						s.push(cur);
						cur = cur->child;
					}
					else {
						flag = true;
						while (cur) {
							valueJsonToString(cur, result, s.top()->type == Type::Object);
							cur = cur->brother;
							if (cur && cur->child) {
								flag = false;
								break;
							}
						}
						if (flag && !s.empty()) {
							if (stringEndWith(result, ",")) {
								result.insert(result.length() - 1, s.top()->type == Type::Object ? "}" : "]");
							}
							else
								result.append(s.top()->type == Type::Object ? "}," : "],");
						}
					}
				}
				else if (!s.empty()) {
					cur = s.top()->brother;
					s.pop();
					flag = true;
					while (cur) {
						if (cur->type == Type::Object || cur->type == Type::Array) {
							flag = false;
							break;
						}
						valueJsonToString(cur, result, s.top()->type == Type::Object);
						cur = cur->brother;
					}
					if (flag && !s.empty()) {
						if (stringEndWith(result, ",")) {
							result.insert(result.length() - 1, s.top()->type == Type::Object ? "}" : "]");
						}
						else
							result.append(s.top()->type == Type::Object ? "}," : "],");
					}
				}
			}
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

			Json parse_number() {
				Json rs(Type::Number);
				size_t start_pos = i;

				if (i < str.size() && str[i] == '-')
					i++;

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

				if (i == str.size()) {
					rs.valueNumber = std::strtod(str.c_str() + start_pos, nullptr);
					return rs;
				}

				if (str[i] != '.' && str[i] != 'e' && str[i] != 'E'
					&& (i - start_pos) <= static_cast<size_t>(std::numeric_limits<int>::digits10)) {
					rs.valueNumber = (double)std::atoi(str.c_str() + start_pos);
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

				rs.valueNumber = std::strtod(str.c_str() + start_pos, nullptr);
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
				struct Frame {
					Json* container;
					string key;
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
					if (parent.container->type == Type::Object)
						done->name = std::move(parent.key);
					parent.container->appendNodeToJson(done);
					state = parent.childDone;
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
					if (top.container->type == Type::Object)
						val->name = std::move(top.key);
					top.container->appendNodeToJson(val);
					state = top.childDone;
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
							stk.push_back({ new Json(Type::Object), {}, PState::OBJ_COMMA_OR_END });
							state = PState::OBJ_KEY_OR_END;
							break;
						}
						// ---- open array ----
						if (ch == '[') {
							i++;
							if (stk.size() > static_cast<size_t>(max_depth))
								return fail("exceeded maximum nesting depth");
							stk.push_back({ new Json(Type::Array), {}, PState::ARR_COMMA_OR_END });
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
							string s = parse_string();
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
						stk.back().key = parse_string();
						if (failed) return Json(Type::Error);
						state = PState::OBJ_COLON;
						break;
					}

					case PState::OBJ_KEY: {
						if (ch != '"')
							return fail("expected '\"' in object, got " + esc(ch));
						i++;
						stk.back().key = parse_string();
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

	class JsonIterator
	{
		Json* ptr;
	public:
		explicit JsonIterator(const Json& p) {
			ptr = p.child;
		}
		explicit JsonIterator(Json* p) {
			ptr = p;
		}
		JsonIterator operator*() const {
			return *this; 
		}
		JsonIterator& operator++() {
			ptr = ptr->brother;
			return *this;
		}
		const JsonIterator operator++(int) {
			return ++(*this);
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

		string key() {
			return ptr->name;
		}

		Json value() {
			return *ptr;
		}
	};

}