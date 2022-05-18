#pragma once
#include <string>
#include "utils.hpp"
#include <variant>
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
		std::variant <int, bool, double, string> data;
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
					else if (Utils::stringEqualTo(typeStr, "char"))
						dd = std::any_cast<char>(data);
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
				else {
					return false;
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
				string v = std::get<std::string>(json.data);
				result += "\"" + json.name + "\":\"" + v + "\",";
			}
			else if (json.type == Type::Number) {
				string intOrDoub = "";
				double temp = std::get<double>(json.data);
				if (temp == (int)temp)
					intOrDoub = std::to_string((int)temp);
				else {
					intOrDoub = std::to_string(temp);
					intOrDoub.erase(intOrDoub.find_last_not_of('0') + 1);
				}

				result += "\"" + json.name + "\":" + intOrDoub + ",";
			}
			else if (json.type == Type::True || json.type == Type::False) {
				result += "\"" + json.name + "\":" + (std::get<bool>(json.data) ? "true" : "false") + ",";
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