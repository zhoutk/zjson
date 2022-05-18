#pragma once
#include <string>
#include "check_type.hpp"
#include "utils.hpp"
#include <any>

namespace ZJSON {

	static const int max_depth = 100;

    using std::string;
    using std::move;

	enum class Type {
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

	class Json {
		Json *next;
		Json *prev;
		Json *child;
		Type type;
		std::any data;
		string name;
	public:
		Json() {
			this->next = nullptr;
			this->prev = nullptr;
			this->child = nullptr;
			this->name = "";
			this->type = Type::Object;
		}

        Json(Type type) : Json(){
			this->type = type;
        }

		Json(Type type, string value) : Json(type) {
            this->data = value.c_str();
		}

		template<typename T> bool AddValue(string name, T value) {
			if (this->type == Type::Object || this->type == Type::Array) {
				string typeStr = check_type<T>();
				Json* node = new Json();
				node->name = name;
				node->data = value;
				if (Utils::stringEqualTo(typeStr, "int")) {
					node->type = Type::Number;
				}
				else if (Utils::stringStartWith(typeStr, "char const") || Utils::stringContain(typeStr, "::basic_string<")) {
					node->type = Type::String;
				}
				
				if (this->child) {
					Json* prev = this->child;
					Json* cur = this->child->next;
					while (cur) {
						prev = cur;
						cur = cur->next;
					}
					node->prev = prev;
					prev->next = node;
				}
				else {
					node->prev = this;
					this->child = node;
				}
				
				return true;
			}
			else {
				return false;
			}
		}

		void toString(Json json, string & result) {
			if (json.type == Type::Object) {
				result.append("{");
				if (json.child)
					toString(*this->child, result);
				if (Utils::stringEndWith(result, ","))
					result = result.substr(0, result.length() - 1);
				result += "}";
			}
			else if (json.type == Type::String) {
				string v;
				if (Utils::stringStartWith(json.data.type().name(), "char const"))
					v = std::any_cast<char const *>(json.data);
				else
					v = std::any_cast<string>(json.data);
				result += "\"" + json.name + "\":\"" + v + "\",";
			}
			else if (json.type == Type::Number) {
				result += "\"" + json.name + "\":" + std::to_string(std::any_cast<int>(json.data)) + ",";
			}
			if (json.next) {
				toString(*json.next, result);
			}
		}

		string toString() {
			string result;
			this->toString(*this, result);
			return result;
		}

	};

}