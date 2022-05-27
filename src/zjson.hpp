#pragma once
#include <string>
#include "utils.hpp"
#include <variant>
#include <any>

namespace ZJSON {

	static const int max_depth = 100;

    using std::string;
    using std::move;

	enum class JsonType
	{
		Object = 6,
		Array = 7
	};

	class Json final {
		enum Type {
			Error,
			False,
			True,
			Null,
			Number,
			String,
			Object,
			Array
		};

		Json* brother;
		Json* child;
		Type type;
		std::variant <int, bool, double, string> data;
		string name;

		static Json parse(const std::string & in, std::string & err);
		static Json parse(const char * in, std::string & err) {
			if (in) {
				return parse(std::string(in), err);
			} else {
				err = "null input";
				return nullptr;
			}
		}

	private:
		Json(Type type) {
			this->brother = nullptr;
			this->child = nullptr;
			this->name = "";
			this->type = type;
		}

	public:
		Json(JsonType type = JsonType::Object) {
			this->brother = nullptr;
			this->child = nullptr;
			this->name = "";
			this->type = (Type)type;
		}

		Json(const Json& origin) {
			this->brother = nullptr;
			this->child = nullptr;
			if(origin.type == Type::Array || origin.type == Type::Object){
				this->name = "";
				this->type = origin.type;
				addSubJson(this, origin.name, origin.child);
			}else{
				this->name = origin.name;
				this->data = origin.data;
				this->type = origin.type;
			}
		}

		Json(string jsonStr) : Json(Type::Error){
			compactJsonString(jsonStr);
			parse(this, jsonStr, 0, 0, this->type == Type::Object);
		}

		bool parse(Json * rs, const string& src, int index, int deep = 0, bool isObj = true){
			if(deep == 0){
				if(src[index] == '{'){
					getJsonObject(rs, src, ++index);
				}
			}else{

			}
			return true;
		}

		bool getJsonObject(Json* rs, const string& src, int& index){
			if(src[index] == '"'){
				int colonIndex = src.find_first_of(':', index);
				int quotationNext = src.find_first_of('"', colonIndex + 2);
			}
			return true;
		}

		void compactJsonString(string& str){

		}

		~Json(){
			if(child){
				DeleteJson(child);
				child = nullptr;
			}
		}

		Json& operator = (const Json& origin) {
			new (this)Json(origin);
			return(*this);
		}

		Json operator[](const int& index) {
			Json rs(Type::Error);
			if(this->type == Type::Array) {
				if(index < 0){
					return rs;
				}else{
					return this->child->find(index);
				}
			}
			else
				return rs;
		}

		Json operator[](const string& key) {
			Json rs(Type::Error);
			if(this->type == Type::Object) {
				if(key.empty()){
					return rs;
				}else{
					return this->child->find(key);
				}
			}
			else
				return rs;
		}

		bool AddValueJson(Json& obj){
			if(this->type == Type::Array){
				return AddValueJson("", obj);
			}else{
				return false;
			}
		}

		bool AddValueJson(string name, Json& obj) {
			if (this->type == Type::Object || this->type == Type::Array) {
				addSubJson(this, this->type == Type::Object ? name : "", &obj);
				return true;
			}
			else {
				return false;
			}
		}

		template<typename T> bool AddValueBase(T value) {
			if(this->type == Type::Array)
				return AddValueBase("", value);
			else
				return false;
		}

		template<typename T> bool AddValueBase(string name, T value) {
			if (this->type == Type::Object || this->type == Type::Array) {
				string typeStr = GetTypeName(T);
				std::cout << "The key : " << name << " ; the type string : " << typeStr << std::endl;
				Json* node = new Json();
				node->name = name;
				std::any data = value;
				if (Utils::stringEqualTo(typeStr, "int") || 
					Utils::stringEqualTo(typeStr, "double") ||
					Utils::stringEqualTo(typeStr, "char") ||
					Utils::stringEqualTo(typeStr, "long") ||
					Utils::stringEqualTo(typeStr, "__int64") ||
					Utils::stringEqualTo(typeStr, "long long") ||
					Utils::stringEqualTo(typeStr, "float")
					) {
					node->type = Type::Number;
					double dd = 0.0;
					if(Utils::stringEqualTo(typeStr, "int"))
						dd = std::any_cast<int>(data);
					else if(Utils::stringEqualTo(typeStr, "float"))
						dd = std::any_cast<float>(data);
					else if (Utils::stringEqualTo(typeStr, "char")) {
						dd = std::any_cast<char>(data);
					}
					else if (Utils::stringEqualTo(typeStr, "long"))
						dd = std::any_cast<long>(data);
					else if (Utils::stringEqualTo(typeStr, "__int64") || Utils::stringEqualTo(typeStr, "long long"))
						dd = std::any_cast<long long>(data);
					else
						dd = std::any_cast<double>(data);
					node->data = dd;
				}
				else if (Utils::stringStartWith(typeStr, "char const") || Utils::stringContain(typeStr, "::basic_string<")) {
					node->type = Type::String;
					string v;
					if (Utils::stringStartWith(typeStr, "char const"))
						v = std::any_cast<char const*>(data);
					else
						v = std::any_cast<string>(data);
					node->data = v;
				}
				else if (Utils::stringEqualTo(typeStr, "bool")) {
					bool dd = std::any_cast<bool>(data);
					if (dd)
						node->type = Type::True;
					else
						node->type = Type::False;
					node->data = dd;
				}
				else if (Utils::stringContain(typeStr, "nullptr")) {
					node->type = Type::Null;
				}
				else {
					return false;
				}
				
				appendNodeToJson(node);
				
				return true;
			}
			else {
				return false;
			}
		}

		string toString() {
			if (this->type == Type::Error)
				return "";
			else {
				string result;
				this->toString(this, result, 0, this->type == Type::Object);
				if(this->type == Type::String)
					return result.substr(1, result.length() - 3);
				else
					return Utils::stringEndWith(result, ",") ? result.substr(0, result.length() - 1) : result;
			}
		}

		bool isError(){
			return this->type == Type::Error;
		}

		bool isNull(){
			return this->type == Type::Null;
		}

		bool isObject(){
			return this->type == Type::Object;
		}

		bool isArray(){
			return this->type == Type::Array;
		}

		bool isNumber(){
			return this->type == Type::Number;
		}

		bool isTrue(){
			return this->type == Type::True;
		}

		bool isFalse(){
			return this->type == Type::False;
		}

		float toFloat(){
			return (float)this->toDouble();
		}

		int toInt(){
			return (int)this->toDouble();
		}

		double toDouble(){
			if(this->type == Type::Number){
				double * rs = std::get_if<double>(&this->data);
				if(rs)
					return (*rs);
				else
					return 0.0;
			}else
				return 0.0;
		}

		bool toBool(){
			if(this->type == Type::False || this->type == Type::True){
				bool * rs = std::get_if<bool>(&this->data);
				if(rs)
					return (*rs);
				else
					return false;
			}else
				return false;
		}

	private:
		void appendNodeToJson(Json* node, Json * self = nullptr)
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

		void addSubJson(Json* self, string name, Json* obj) {
			if(self == nullptr || obj == nullptr)
				return;
			if (obj->type == Type::Object || obj->type == Type::Array)
			{
				Json *subObj = new Json();
				subObj->type = obj->type;
				subObj->name = name;
				appendNodeToJson(subObj, self);
				if(obj->child)
					addSubJson(subObj, "", obj->child);
			}
			else
			{
				Json* cur = obj;
				while (cur)
				{
					if (cur->type == Type::Object || cur->type == Type::Array)
						addSubJson(self, cur->name, cur);
					else
					{
						Json *subContent = new Json();
						subContent->type = cur->type;
						subContent->name = cur->name;
						subContent->data = cur->data;
						appendNodeToJson(subContent, self);
					}
					cur = cur->brother;
				}
			}
		}

		void DeleteJson(Json *obj) {		//all json type is value type
			Json *cur = obj;
			Json *follow = obj;
			do{
				cur = follow;
				follow = follow->brother;
				if (cur->type == Type::Object || cur->type == Type::Array) {
					if (cur->child){
						DeleteJson(cur->child);
						cur->child = nullptr;
					}
				}
				delete cur;
				cur = nullptr;
			}while(follow);
		}

		Json find(int index){
			int ct = 0;
			Json* cur = this;
			Json rs(Type::Error);
			while (cur && ct < index)
			{
				cur = cur->brother;
				ct++;
			}
			if(ct < index || !cur)
				return rs;
			else
				return *cur;
		}

		Json find(const string& key, bool notArray = true){
			if(this->type == Type::Array || this->type == Type::Object){
				if(this->child)
					return this->child->find(key, notArray);
				else
					return Json(Type::Error);
			}else{
				Json* cur = this;
				Json rs(Type::Error);
				while (cur)
				{
					if(notArray && cur->name == key){
						return Json(*cur);
					}
					else{
						if(cur->type == Type::Array || cur->type == Type::Object){
							if(cur->child){
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

		void toString(Json* json, string & result, int deep = 0, bool isObj = true) {
			if (json->type == Type::Object || json->type == Type::Array) {
				if(deep > 0)
					result.append(isObj ? "\"" + json->name + "\":" : "")
					.append(json->type == Type::Object ? "{" : "[");
				else
					result.append(json->type == Type::Object ? "{" : "[");
				if (json->child)
					toString(json->child, result, deep + 1, json->type == Json::Type::Object);
				if (Utils::stringEndWith(result, ","))
					result = result.substr(0, result.length() - 1);
				if(deep > 0)
					result += (json->type == Type::Object ? "}," : "],");
				else
					result += (json->type == Type::Object ? "}" : "]");
			}
			else if (json->type == Type::String) {
				string v = std::get<std::string>(json->data);
				result += (isObj ? "\"" + json->name + "\":\"" : "\"") + v + "\",";
			}
			else if (json->type == Type::Number) {
				string intOrDoub = "";
				double temp = std::get<double>(json->data);
				if (temp == (int)temp)
					intOrDoub = std::to_string((int)temp);
				else {
					intOrDoub = std::to_string(temp);
					intOrDoub.erase(intOrDoub.find_last_not_of('0') + 1);
				}

				result += (isObj ? "\"" + json->name + "\":" : "") + intOrDoub + ",";
			}
			else if (json->type == Type::True || json->type == Type::False) {
				result += (isObj ? "\"" + json->name + "\":" : "") + (std::get<bool>(json->data) ? "true" : "false") + ",";
			}
			else if (json->type == Type::Null) {
				result += (isObj ? "\"" + json->name + "\":" : "") + "null,";
			}

			if (json->brother) {
				toString(json->brother, result, deep, isObj);
			}

		}

		static inline string esc(char c) {
			char buf[12];
			if (static_cast<uint8_t>(c) >= 0x20 && static_cast<uint8_t>(c) <= 0x7f) {
				snprintf(buf, sizeof buf, "'%c' (%d)", c, c);
			} else {
				snprintf(buf, sizeof buf, "(%d)", c);
			}
			return string(buf);
		}

		struct JsonParser final {

			const string &str;
			size_t i;
			string &err;
			bool failed;

			Json fail(string &&msg) {
				Json err(Type::Error);
				err.name = msg;
				return fail(move(msg), err);
			}

			template <typename T> T fail(string &&msg, const T err_ret) {
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
					if (i > str.size()-2)
						return fail("unexpected end of input inside multi-line comment", false);
					while (!(str[i] == '*' && str[i+1] == '/')) {
						i++;
						if (i > str.size()-2)
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
				}
				while(comment_found);
			}

			char get_next_token() {
				consume_garbage();
				if (failed) return static_cast<char>(0);
				if (i == str.size())
					return fail("unexpected end of input", static_cast<char>(0));

				return str[i++];
			}

			Json parse_json(int depth) {
				if (depth > max_depth) {
					return fail("exceeded maximum nesting depth");
				}

				char ch = get_next_token();
				if (failed)
					return Json();



				return fail("expected value, got " + esc(ch));
			}

		};

	};

	Json Json::parse(const string &in, string &err) {
		JsonParser parser { in, 0, err, false };
		Json result = parser.parse_json(0);

		// Check for any trailing garbage
		parser.consume_garbage();
		if (parser.failed)
			return Json();
		if (parser.i != in.size())
			return parser.fail("unexpected trailing " + esc(in[parser.i]));

		return result;
	}
}