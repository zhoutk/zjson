#include <iostream>
#include "zjson.hpp"
#include <array>
#include <vector>

using namespace ZJSON;

int main(int argc, char* argv[])
{
	for(int i = 0; i < 1; i++) {
		Json ajson(JsonType::Object);
		std::string data = "kevin";
		float f = 9.01234567;
		long l = 123;
		long long ll = 56789;
		ajson.AddValueBase("long", l);
		ajson.AddValueBase("longlong", ll);
		ajson.AddValueBase("sex", true);
		ajson.AddValueBase("falt", false);
		ajson.AddValueBase("name", data);
		ajson.AddValueBase("school-cn", "第八十五中学");
		ajson.AddValueBase("school-en", "the 85th.");
		ajson.AddValueBase("age", 10);
		ajson.AddValueBase("scores", 95.98);
		ajson.AddValueBase("classroom", f);
		ajson.AddValueBase("index", '6');
		ajson.AddValueBase("nullkey", nullptr);

		Json sub;
		sub.AddValueBase("math", 99);
		ajson.AddValueJson("subJson", sub);

		ajson.AddValueBase("name", data);

		Json subArray(JsonType::Array);
		subArray.AddValueBase("first", "I'm the first one.");
		subArray.AddValueBase("two", 2);
		Json subb;
		subb.AddValueBase("sbbbbb", "bbbbbbb");
		Json littleArray(JsonType::Array);
		littleArray.AddValueBase(888);
		littleArray.AddValueBase(999);
		littleArray.AddValueJson("", subb);
		subb.AddValueJson("arr", littleArray);
		subArray.AddValueJson("subObj", subb);
		ajson.AddValueJson("array", subArray);

		ajson.AddValueBase("scores", 95.98);
		// std::cout << "ajson's string is : " << ajson.toString() << std::endl;

		Json oper = ajson["classroom"];
		Json operBool1 = ajson["sex"];
		Json operBool2 = ajson["fail"];

		std::cout << "[string] operator : " << oper.toString() << std::endl;
		std::cout << "[int] operator : " << oper.toInt() << std::endl;
		std::cout << "[float] operator : " << oper.toFloat() << std::endl;
		std::cout << "[double] operator : " << oper.toDouble() << std::endl;
		std::cout << "[true] operator : " << operBool1.toBool() << std::endl;
		std::cout << "[false] operator : " << operBool2.toBool() << std::endl;

	}

	int a = 1;

	// Json aClone(subb);

	// Json aCp = subArray;

	// std::cout << std::endl;
	// std::cout << "ajson's string is : " << ajson.toString() << std::endl;
	// std::cout << "aClone's string is : " << aClone.toString() << std::endl;
	// std::cout << "aCp's string is : " << aCp.toString() << std::endl;
}