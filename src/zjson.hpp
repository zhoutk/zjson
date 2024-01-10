#pragma once
#include <string>
#include <vector>
#include <stack>
#include <iostream>
#include <algorithm>
#include <limits.h>
#include <array>
#include <cstring>

namespace ZJSON {
	using std::string;
	using std::move;
	using std::vector;
	using std::stack;

	static const int max_depth = 100;
	static const std::array<string, 8> TYPENAMES {"Error", "False", "True", "Null", "Number", "String", "Object", "Array"};
	static const double MinValue = 0.000001;
	static const int DecimalLength = 7;

	static inline bool stringContain(const string& str, const string& to) {
		return str.find(to) != string::npos;
	}

	static inline bool stringEqualTo(const string& str, const string& to) {
		return str.compare(to) == 0;
	}

	static inline bool stringEndWith(const string& str, const string& tail) {
		return str.compare(str.size() - tail.size(), tail.size(), tail) == 0;
	}

	static inline bool stringStartWith(const string& str, const string& head) {
		return str.compare(0, head.size(), head) == 0;
	}

	enum class JsonType
	{
		Object = 6,
		Array = 7
	};

	static char globBuffer[100];
	static long long globIntVar;
	static bool globBoolVar;
	static char globDecimalVar[DecimalLength] = { '\0', '\0', '\0' , '\0' , '\0' , '\0', '\0' };

	static void floatToCharStar(double data, char* buffer) {
		data = std::abs(data);
		data -= (long long)data;
		int ct = 0;
		memset(buffer, '\0', DecimalLength);
		double minValue = MinValue;
		while (!(std::abs(data - 1) < minValue || std::abs(data) < minValue) && ct < 6) {
			data *= 10;
			double nextDataVal = data - (int)data;
			if ((std::abs(nextDataVal - 1) < minValue || std::abs(nextDataVal) < minValue || ct == 5) && data < 9 && nextDataVal > 0.5) {
				buffer[ct] = data + 49;
				break;
			}
			else
				buffer[ct] = data + 48;
			data = nextDataVal;
			ct++;
			minValue *= 10;
		}
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
	private:
		Json* brother;
		Json* child;
		Type type;
		string valueString;
		double valueNumber;
		string name;

		static Json parse(const std::string& in, std::string& err)
		{
			JsonParser parser{ in, 0, err, false };
			Json result = parser.parse_json(0);
			if (result.type == Type::Error)
				return result;
			parser.consume_garbage();
			if (parser.i != in.size())
				return parser.fail("unexpected trailing " + esc(in[parser.i]));
			return result;
		}

		static Json parse(const char* in, std::string& err) {
			if (in) {
				return parse(std::string(in), err);
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
			this->type = type;
			this->valueNumber = 0;
		}

	public:
		Json(JsonType type = JsonType::Object) {
			this->brother = nullptr;
			this->child = nullptr;
			this->type = (Type)type;
		}

		template<typename T> Json(const T& value) {
			this->brother = nullptr;
			this->child = nullptr;
			this->valueNumber = value;
			this->type = Type::Number;
		}

		Json(const float& value) {
			if (std::isnan(value)) {
				this->brother = nullptr;
				this->child = nullptr;
				this->type = Type::Null;
			} else {
				this->brother = nullptr;
				this->child = nullptr;
				this->valueNumber = value;
				this->type = Type::Number;
			}
		}

		Json(const double& value) {
			if (std::isnan(value)) {
				this->brother = nullptr;
				this->child = nullptr;
				this->type = Type::Null;
			} else {
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
			new (this)Json(string(jsonStr));
		}

		Json(const bool& value) {
			this->brother = nullptr;
			this->child = nullptr;
			this->type = value ? Type::True : Type::False;
		}

		Json(const std::nullptr_t&) {
			this->brother = nullptr;
			this->child = nullptr;
			this->type = Type::Null;
		}

		Json(const Json& origin) {
			this->brother = nullptr;
			this->child = nullptr;
			this->type = origin.type;
			if (origin.type == Type::Array || origin.type == Type::Object) {
				this->name = origin.name;
				addValueJson(origin.child ? origin.child->name : "", *origin.child);
			}
			else {
				this->name = origin.name;
				this->valueString = origin.valueString;
				this->valueNumber = origin.valueNumber;
			}
		}

		Json(Json&& rhs) {
			this->type = rhs.type;
			this->child = rhs.child;
			this->brother = rhs.brother;
			this->name = rhs.name;
			this->valueString = rhs.valueString;
			this->valueNumber = rhs.valueNumber;
			rhs.child = nullptr;
			rhs.brother = nullptr;
		}

		explicit Json(std::initializer_list<std::pair<const std::string, Json>> values) {
			this->child = nullptr;
			this->brother = nullptr;
			this->type = Type::Object;
			for (auto al : values) {
				al.second.name = al.first;
				this->extendItem(&al.second);
			}
		}

		~Json() {
			if (child) {
				deleteJson(child);
				child = nullptr;
			}
		}

		Json& operator = (const Json& origin) {
			new (this)Json(origin);
			return(*this);
		}

		Json& operator = (Json&& rhs) {
			new (this)Json(std::move(rhs));
			return(*this);
		}

		Json operator[](const int& index) const {
			Json rs(Type::Error);
			if (this->type == Type::Array) {
				if (index < 0) {
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
			Json rs(Type::Error);
			if (this->type == Type::Object) {
				if (key.empty() || this->child == nullptr) {
					return rs;
				}
				else {
					if (this->child->name == key)
						return *(this->child);
					else
						return this->child->find(key);
				}
			}
			else
				return rs;
		}

		bool contains(const string& key) const {
			return !((*this)[key].type == Type::Error);
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
			if (!name.empty() && this->type == Type::Object || this->type == Type::Array) {
				Json* node = new Json(value);
				node->name = this->type == Type::Object ? name : "";
				appendNodeToJson(node);
			}
			return (*this);
		}

		Json& add(string name, const Json& value) {
			if (!name.empty() && this->type == Type::Object || this->type == Type::Array) {
				Json* node = new Json(value);
				node->name = this->type == Type::Object ? name : "";
				appendNodeToJson(node);
			}
			return (*this);
		}

		Json& add(string name, Json&& value) {
			if (!name.empty() && this->type == Type::Object || this->type == Type::Array) {
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

		float toFloat() const {
			return (float)this->toDouble();
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
				if (this->type == Type::String)
					return result.substr(1, result.length() - 3);
				else
					return stringEndWith(result, ",") ? result.erase(result.length() - 1) : result;
			}
		}

		int toInt() const {
			return (int)this->toDouble();
		}

		double toDouble() const {
			if (this->type == Type::Number) {
				return valueNumber;
			}
			else if (this->type == Type::String) {
				return atof(this->toString().c_str());
			}
			else
				return 0.0;
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
			else {
				return rs;
			}
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
			}
			return (*this);
		}

		Json& remove(const string& key, Json* self = nullptr, Json* prev = nullptr)
		{
			if (key.empty() || (self == nullptr && this->type != Type::Object))
				return (*this);
			if (self == nullptr)
				self = this;
			Json* cur = self;
			Json* pre = self;
			if (prev)
				pre = prev;
			bool found = false;
			do
			{
				if (cur->name == key)
				{
					if (cur->type == Type::Array || cur->type == Type::Object) {
						pre->brother = cur->brother;
					}
					else if (pre->type == Type::Array || pre->type == Type::Object)
						pre->child = cur->brother;
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
			if (self->child) {
				Json* prev = self->child;
				Json* cur = self->child->brother;
				while (cur) {
					prev = cur;
					cur = cur->brother;
				}
				prev->brother = node;
			}
			else {
				self->child = node;
			}
		}

		bool addValueJson(string name, Json& obj) {
			if (this->type == Type::Object || this->type == Type::Array) {
				addSubJson(this, this->type == Type::Object ? name : "", &obj);
				return true;
			}
			else {
				return false;
			}
		}

		void addSubJson(Json* self, string name, Json* obj) {	//only use by addValueJson, for recursion
			if (self == nullptr || obj == nullptr)
				return;
			Json* cur = obj;
			while (cur)
			{
				if (cur->type == Type::Object || cur->type == Type::Array) {
					if (cur->child == nullptr)
						return;
					Json* subObj = new Json();
					subObj->type = cur->type;
					subObj->name = name;
					appendNodeToJson(subObj, self);
					addSubJson(subObj, cur->child->name, cur->child);
				}
				else
				{
					Json* subContent = new Json();
					subContent->type = cur->type;
					subContent->name = cur->name;
					subContent->valueNumber = cur->valueNumber;
					subContent->valueString = cur->valueString;
					appendNodeToJson(subContent, self);
				}
				cur = cur->brother;
				if (cur)
					name = cur->name;
			}
		}

		void deleteJson(Json* obj) {		//all json type is value type
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
				delete cur;
				cur = nullptr;
			} while (follow);
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
					result.append("\"").append(json->name).append("\":");
				result.append("\"").append(json->valueString).append("\",");
			}
			else if (json->type == Type::Number) {
				if (isObj)
					result.append("\"").append(json->name).append("\":");
				if (json->valueNumber > LLONG_MAX || json->valueNumber < LLONG_MIN) {
					sprintf(globBuffer, "%e", json->valueNumber);
					result.append(globBuffer);
				}
				else {
					globIntVar = (long long)json->valueNumber;
					sprintf(globBuffer, "%lld", globIntVar);
					result.append(globBuffer);
					if (std::abs(json->valueNumber - globIntVar) >= MinValue) {
						floatToCharStar(json->valueNumber, globDecimalVar);
						result.append(".").append(globDecimalVar);
					}
				}
				result.append(",");
			}
			else if (json->type == Type::True) {
				if (isObj)
					result.append("\"").append(json->name).append("\":");
				result.append("true,");
			}
			else if (json->type == Type::False) {
				if (isObj)
					result.append("\"").append(json->name).append("\":");
				result.append("false,");
			}
			else if (json->type == Type::Null) {
				if (isObj)
					result.append("\"").append(json->name).append("\":");
				result.append("null,");
			}
		}

		//void toString(const Json* json, string& result, int deep = 0, bool isObj = true) const {
		//	if (json->type == Type::Object || json->type == Type::Array) {
		//		if (deep > 0)
		//			result.append(isObj ? "\"" + json->name + "\":" : "")
		//			.append(json->type == Type::Object ? "{" : "[");
		//		else
		//			result.append(json->type == Type::Object ? "{" : "[");
		//		if (json->child)
		//			toString(json->child, result, deep + 1, json->type == Type::Object);
		//		if (stringEndWith(result, ","))
		//			result = result.substr(0, result.length() - 1);

		//		if (deep > 0)
		//			result += (json->type == Type::Object ? "}," : "],");
		//		else
		//			result += (json->type == Type::Object ? "}" : "]");
		//	}
		//	else
		//		valueJsonToString(json, result, isObj);

		//	if (json->brother) {
		//		toString(json->brother, result, deep, isObj);
		//	}
		//}

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
						result.append(cur->name.empty() ? "" : "\"" + cur->name + "\":")
							.append(cur->type == Type::Object ? "{" : "[");
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

			Json fail(string&& msg) {
				Json err(Type::Error);
				err.name = msg;
				return fail(std::move(msg), err);
			}

			template <typename T> T fail(string&& msg, const T err_ret) {
				//std::cout << std::endl << " --- Error When Parsing --- " << msg << std::endl;
				if (!failed)
					err = std::move(msg);
				failed = true;
				return err_ret;
			}

			void consume_whitespace() {
				while (str[i] == ' ' || str[i] == '\r' || str[i] == '\n' || str[i] == '\t')
					i++;
			}

			bool consume_comment() {
				bool comment_found = false;
				if (str[i] == '/') {
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

			char get_next_token() {
				consume_garbage();
				if (failed) return static_cast<char>(0);
				if (i == str.size())
					return fail("unexpected end of input", static_cast<char>(0));

				return str[i++];
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

				if (str[i] == '-')
					i++;

				if (str[i] == '0') {
					i++;
					if (in_range(str[i], '0', '9'))
						return fail("leading 0s not permitted in numbers");
				}
				else if (in_range(str[i], '1', '9')) {
					i++;
					while (in_range(str[i], '0', '9'))
						i++;
				}
				else {
					return fail("invalid " + esc(str[i]) + " in number");
				}

				if (str[i] != '.' && str[i] != 'e' && str[i] != 'E'
					&& (i - start_pos) <= static_cast<size_t>(std::numeric_limits<int>::digits10)) {
					rs.valueNumber = (double)std::atoi(str.c_str() + start_pos);
					return rs;
				}

				if (str[i] == '.') {
					i++;
					if (!in_range(str[i], '0', '9'))
						return fail("at least one digit required in fractional part");

					while (in_range(str[i], '0', '9'))
						i++;
				}

				if (str[i] == 'e' || str[i] == 'E') {
					i++;

					if (str[i] == '+' || str[i] == '-')
						i++;

					if (!in_range(str[i], '0', '9'))
						return fail("at least one digit required in exponent");

					while (in_range(str[i], '0', '9'))
						i++;
				}

				rs.valueNumber = std::strtod(str.c_str() + start_pos, nullptr);
				return rs;
			}

			Json expect(const string& expected, Json res) {
				if (i > 0) {
					i--;
					if (str.compare(i, expected.length(), expected) == 0) {
						i += expected.length();
						return res;
					}
					else {
						return fail("parse error: expected " + expected + ", got " + str.substr(i, expected.length()));
					}
				}
				else {
					return Json(Type::Error);
				}

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

			Json parse_json(int depth) {
				if (depth > max_depth) {
					return fail("exceeded maximum nesting depth");
				}

				char ch = get_next_token();
				if (failed)
					return Json(Type::Error);

				if (ch == '-' || (ch >= '0' && ch <= '9')) {
					i--;
					return parse_number();
				}

				if (ch == 't')
					return expect("true", Json(Type::True));

				if (ch == 'f')
					return expect("false", Json(Type::False));

				if (ch == 'n')
					return expect("null", Json(Type::Null));

				if (ch == '"') {
					Json jsonString(Type::String);
					jsonString.valueString = parse_string();
					return jsonString;
				}

				if (ch == '{') {
					Json data(Type::Object);
					data.child = new Json(Type::Error);
					Json* cur = data.child;
					ch = get_next_token();
					if (ch == '}')
						return data;

					while (1) {
						if (ch != '"')
							return fail("expected '\"' in object, got " + esc(ch));

						string key = parse_string();
						if (failed)
							return Json(Type::Error);

						ch = get_next_token();
						if (ch != ':')
							return fail("expected ':' in object, got " + esc(ch));

						*cur = parse_json(depth + 1);
						cur->name = key;

						if (failed)
							return Json(Type::Error);

						ch = get_next_token();
						if (ch == '}')
							break;
						if (ch != ',')
							return fail("expected ',' in object, got " + esc(ch));

						cur->brother = new Json(Type::Error);
						cur = cur->brother;
						ch = get_next_token();
					}
					return data;
				}

				if (ch == '[') {
					Json data(Type::Array);
					data.child = new Json(Type::Error);
					Json* cur = data.child;
					ch = get_next_token();
					if (ch == ']')
						return data;

					while (1) {
						i--;
						*cur = parse_json(depth + 1);

						if (failed)
							return Json(Type::Error);

						ch = get_next_token();
						if (ch == ']')
							break;
						if (ch != ',')
							return fail("expected ',' in object, got " + esc(ch));

						cur->brother = new Json(Type::Error);
						cur = cur->brother;
						ch = get_next_token();
					}
					return data;
				}

				return fail("expected value, got " + esc(ch));
			}

		};

	};

}