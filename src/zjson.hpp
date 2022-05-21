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
		Object = 7,
		Array = 6
	};

	class Json {
		enum Type {
			Error,
			False,
			True,
			Null,
			Number,
			String,
			Array,
			Object,
			Raw
		};

		Json* next;
		Json* prev;
		Json* child;
		Type type;
		std::variant <int, bool, double, string> data;
		string name;

	private:
		Json(Type type) {
			this->next = nullptr;
			this->prev = nullptr;
			this->child = nullptr;
			this->name = "";
			this->type = type;
		}

	public:
		Json(JsonType type = JsonType::Object) {
			this->next = nullptr;
			this->prev = nullptr;
			this->child = nullptr;
			this->name = "";
			this->type = (Type)type;
		}

		Json(const Json& origin) {
			this->next = nullptr;
			this->prev = nullptr;
			this->child = nullptr;
			this->name = "";
			this->type = origin.type;
			addSubJson(this, origin.name, origin.child);
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

		Json operator[](string key) {
			Json rs;
			return rs;
		}

		bool AddValueJson(string name, Json& obj) {
			if (obj.type == Type::Object || obj.type == Type::Array) {
				addSubJson(this, name, &obj);
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
			string result;
			this->toString(this, result, 0, this->type == Type::Object);
			return result;
		}

	private:
		void appendNodeToJson(Json* node, Json * self = nullptr)
		{
			if (self == nullptr)
				self = this;
			if (self->child) {
				Json* prev = self->child;
				Json* cur = self->child->next;
				while (cur) {
					prev = cur;
					cur = cur->next;
				}
				node->prev = prev;
				prev->next = node;
			}
			else {
				node->prev = nullptr;
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
					cur = cur->next;
				}
			}
		}

		void DeleteJson(Json *obj) {		//all json type is value type
			Json *cur = obj;
			Json *follow = obj;
			do{
				cur = follow;
				follow = follow->next;
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

			if (json->next) {
				toString(json->next, result, deep, isObj);
			}
		}

	};

}