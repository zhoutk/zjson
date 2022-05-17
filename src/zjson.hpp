#pragma once
#include <string>

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
		string data;
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
            this->data = value;
		}

		template<typename T> void AddValue(Type type, string name, T value) {
			Json* node = new Json(type);
			node->name = name;
			switch (type)
			{
			case ZJSON::Type::Error:
				break;
			case ZJSON::Type::False:
				node->data = "false";
				break;
			case ZJSON::Type::True:
				node->data = "true";
				break;
			case ZJSON::Type::Null:
				node->data = "null";
				break;
			case ZJSON::Type::Number:
				node->data = std::to_string(value);
				break;
			case ZJSON::Type::String:
				node->data = value;
				break;
			case ZJSON::Type::Array:
				break;
			case ZJSON::Type::Object:
				break;
			case ZJSON::Type::Raw:
				break;
			default:
				break;
			}
		}

		bool AddValueString(string name, string value) {
			if (this->type == Type::Object || this->type == Type::Array) {
				Json* node = new Json(Type::String, value);
				node->name = name;
				this->child = node;
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
					result += "}";
			}
			else if (json.type == Type::String) {
				result += "\"" + json.name + "\":\"" + json.data + "\"";
			}
			else if (json.type == Type::Number) {
				result += "\"" + json.name + "\":" + json.data;
			}
		}

		string toString() {
			string result;
			this->toString(*this, result);
			return result;
		}

	};

}