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
	string str = "[    \"JSON Test Pattern pass1\",    {\"object with 1 member\":[\"array with 1 element\"]},    {},    [],    -42,    true,    false,    null,    {        \"integer\": 1234567890,        \"real\": -9876.543210,        \"e\": 0.123456789e-12,        \"E\": 1.234567890E+34,        \"\":  23456789012E66,        \"zero\": 0,        \"one\": 1}]";
	Json objStr(str);
	std::cout << "parse a object string : " << objStr.toString() << std::endl;
	// str = "[\"true\":true,\"false\":false,\"null\":null,\"age\":18,\"score\":12.3456,\"name\":\"kevin\"]";
	// Json arrStr(str);
	// std::cout << "parse a array string : " << arrStr.toString() << std::endl;
	// for(int i = 0; i < 1; i++) {
	// 	Json ajson(JsonType::Object);
	// 	std::string data = "kevin";
	// 	float f = 9.01234567;
	// 	long l = 123;
	// 	long long ll = 56789;
	// 	ajson.AddValueBase("long", l);
	// 	ajson.AddValueBase("longlong", ll);
	// 	ajson.AddValueBase("sex", true);
	// 	ajson.AddValueBase("falt", false);
	// 	ajson.AddValueBase("name", data);
	// 	ajson.AddValueBase("school-cn", "第八十五中学");
	// 	ajson.AddValueBase("school-en", "the 85th.");
	// 	ajson.AddValueBase("age", 10);
	// 	ajson.AddValueBase("scores", 95.98);
	// 	ajson.AddValueBase("classroom", f);
	// 	ajson.AddValueBase("index", '6');
	// 	ajson.AddValueBase("nullkey", nullptr);

	// 	Json sub;
	// 	sub.AddValueBase("math", 99);
	// 	ajson.AddValueJson("subJson", sub);

	// 	Json subArray(JsonType::Array);
	// 	subArray.AddValueBase("first", "I'm the first one.");
	// 	subArray.AddValueBase("two", 2);
	// 	Json subb;
	// 	subb.AddValueBase("sbbbbb", "bbbbbbb");
	// 	Json littleArray(JsonType::Array);
	// 	littleArray.AddValueBase(888);
	// 	littleArray.AddValueBase(999);
	// 	Json sub2;
	// 	sub2.AddValueBase("sb2", "second");
	// 	littleArray.AddValueJson(sub2);
	// 	subb.AddValueJson("arr", littleArray);
	// 	subArray.AddValueJson("subObj", subb);
	// 	ajson.AddValueJson("array", subArray);

	// 	ajson.AddValueBase("scores", 95.98);
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