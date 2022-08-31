#include <iostream>
#include "zjson.hpp"
#include <array>
#include <vector>

using namespace ZJSON;

int main(int argc, char* argv[])
{
	//string str = "{\"array\":[\"first\",90.12387,null,2,true],\"true\":true,\"false\":false,\"null\":null,\"age\":18,\"score\":12.3456,\"name\":\"kevin\"}";
	//string str = "{\"age\":10,\"array\":[\"first\",null],\"true\":true}"; //,\"true\":true
	//string str = "{\"long\":123,\"longlong\":56789,\"sex\":true,\"name\":\"kevin\",\"school-cn\":\"第八十五中学\",\"subObjct\":{\"first\":\"this is the first.\",\"second obj\":{\"sub2-1\":\"the second sub object.\",\"sub2-2\":\"the second field.\"},\"a number\":666},\"school-en\":\"the 85th.\",\"age\":10,\"scores\":95.98,\"classroom\":9.012345,\"index\":54,\"nullkey\":null}";
	//string str = "{\"subObjct\":{\"first\":\"this is the first.\",\"second obj\":{\"array01\":[1,2,3,{\"sub3\":\"sub3-1\",\"sub3-next\":true}]}}}";
	//string str = "[    \"JSON Test Pattern pass1\",    {\"object with 1 member\":[\"array with 1 element\"]},    {},    [],    -42]";
	//string str = "[    \"JSON Test Pattern pass1\",    {\"object with 1 member\":[\"array with 1 element\"]},    {},    [],    -42,    true,    false,    null,    {        \"integer\": 1234567890,        \"real\": -9876.543210,        \"e\": 0.123456789e-12,        \"E\": 1.234567890E+34,        \"\":  23456789012E66,        \"zero\": 0,        \"one\": 1}]";
	Json subArray(JsonType::Array);
	subArray.AddSubitem(2);
	subArray.AddSubitem({12,13,14,15});
	std::vector<Json> sub = subArray.toVector();
	// Json sub2;
	// sub2.AddSubitem("math", 99);
	// sub2.AddSubitem("str", "a string.");
	// subArray.AddSubitem(sub2);
	// Json sub{{"fkey", false},{"strkey","ffffff"},{"nkey", nullptr}, {"n1", 2}, {"num2", 9.98}, {"okey", subArray}};
	// sub.AddSubitem("this is the first.");
	// Json subb(JsonType::Array);
	// subb.AddSubitem("the second sub object.");
	// subb.AddSubitem("the second field.");
	// sub.AddSubitem("second obj", subb);
	std::cout << "parse a object string : " << std::endl;
	// str = "[\"true\":true,\"false\":false,\"null\":null,\"age\":18,\"score\":12.3456,\"name\":\"kevin\"]";
	// Json arrStr(str);
	// std::cout << "parse a array string : " << arrStr.toString() << std::endl;
	// for(int i = 0; i < 1; i++) {
	// 	Json ajson(JsonType::Object);
	// 	std::string data = "kevin";
	// 	float f = 9.01234567;
	// 	long l = 123;
	// 	long long ll = 56789;
	// 	ajson.AddSubitem("long", l);
	// 	ajson.AddSubitem("longlong", ll);
	// 	ajson.AddSubitem("sex", true);
	// 	ajson.AddSubitem("falt", false);
	// 	ajson.AddSubitem("name", data);
	// 	ajson.AddSubitem("school-cn", "第八十五中学");
	// 	ajson.AddSubitem("school-en", "the 85th.");
	// 	ajson.AddSubitem("age", 10);
	// 	ajson.AddSubitem("scores", 95.98);
	// 	ajson.AddSubitem("classroom", f);
	// 	ajson.AddSubitem("index", '6');
	// 	ajson.AddSubitem("nullkey", nullptr);

	// 	Json sub;
	// 	sub.AddSubitem("math", 99);
	// 	ajson.AddValueJson("subJson", sub);

	// 	Json subArray(JsonType::Array);
	// 	subArray.AddSubitem("first", "I'm the first one.");
	// 	subArray.AddSubitem("two", 2);
	// 	Json subb;
	// 	subb.AddSubitem("sbbbbb", "bbbbbbb");
	// 	Json littleArray(JsonType::Array);
	// 	littleArray.AddSubitem(888);
	// 	littleArray.AddSubitem(999);
	// 	Json sub2;
	// 	sub2.AddSubitem("sb2", "second");
	// 	littleArray.AddValueJson(sub2);
	// 	subb.AddValueJson("arr", littleArray);
	// 	subArray.AddValueJson("subObj", subb);
	// 	ajson.AddValueJson("array", subArray);

	// 	ajson.AddSubitem("scores", 95.98);
	// 	std::cout << "ajson's string is : " << ajson.toString() << std::endl;

	// 	Json oper = ajson["sb2"];
	// 	Json operArr = ajson["arr"];
	// 	Json operArr2 = ajson["arr"][2];
	// 	Json operBool1 = ajson["sex"];
	// 	Json operBool2 = ajson["fail"];
	// 	Json nullValue = ajson["nullkey"];

	// 	std::cout << "[sb2] operator : " << oper.toString() << std::endl;
	// 	std::cout << "[arr] operator : " << operArr.toString() << std::endl;
	// 	std::cout << "[arr2] operator : " << operArr2.toString() << std::endl;
	// 	std::cout << "[int] operator : " << oper.toInt() << std::endl;
	// 	std::cout << "[float] operator : " << oper.toFloat() << std::endl;
	// 	std::cout << "[double] operator : " << oper.toDouble() << std::endl;
	// 	std::cout << "[true] operator : " << operBool1.toBool() << std::endl;
	// 	std::cout << "[false] operator : " << operBool2.toBool() << std::endl;
	// 	std::cout << "[null] operator : " << nullValue.isNull() << std::endl;

	// }

	// int a = 1;

	// Json aClone(subb);

	// Json aCp = subArray;

	// std::cout << std::endl;
	// std::cout << "ajson's string is : " << ajson.toString() << std::endl;
	// std::cout << "aClone's string is : " << aClone.toString() << std::endl;
	// std::cout << "aCp's string is : " << aCp.toString() << std::endl;
}