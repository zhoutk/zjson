#include "gtest/gtest.h"
#include "../src/zjson.hpp"
#include <cmath>
#include <array>
#include <vector>

using namespace ZJSON;

TEST(TestArray, test_array_1) {
	Json ajson(JsonType::Array);
	std::string data = "kevin";
	float f = 9.01234567;
	long l = 123;
	long long ll = 56789;
	Json sub(JsonType::Array);
	sub.add("this is the first.");
	Json subb(JsonType::Array);
	subb.add("the second sub object.").add("the second field.");
	sub.add("second obj", subb).add("a number", 666);

	ajson.add("long", l).add("longlong", ll).add("sex", true)
		.add("name", data).add("school-cn", "第八十五中学")
		.add("subArray", sub).add("school-en", "the 85th.")
		.add("age", 10).add("scores", 95.98).add("classroom", f)
		.add("index", '6').add("nullkey", nullptr);

	EXPECT_EQ(ajson.toString(), "[123,56789,true,\"kevin\",\"第八十五中学\",[\"this is the first.\",[\"the second sub object.\",\"the second field.\"],666],\"the 85th.\",10,95.98,9.012345,54,null]");
	EXPECT_EQ(ajson[5].toString(), "[\"this is the first.\",[\"the second sub object.\",\"the second field.\"],666]");

	string multi = "[[[[   [[[[[[[[[[   [[[[[\"Not too deep\"]]]]]]]]]]]]]  \t   ]]]   \n]]]";
	Json arrMulti(multi);
	EXPECT_EQ(arrMulti.toString(), "[[[[[[[[[[[[[[[[[[[\"Not too deep\"]]]]]]]]]]]]]]]]]]]");

	string subEmpty = "[    \"JSON Test Pattern pass1\",    {\"object with 1 member\":[\"array with 1 element\"]},    {},    [],    -42]";
	Json ArrSubEmpty(subEmpty);
	EXPECT_EQ(ArrSubEmpty.toString(), "[\"JSON Test Pattern pass1\",{\"object with 1 member\":[\"array with 1 element\"]},{},[],-42]");

	string eStr = "[12345.6789e-7,12345.6789e-3,12345.6789e2,12345.6789e8]";
	Json objeStr(eStr);
	EXPECT_EQ(objeStr.toString(), "[0.001235,12.345679,1234567.89,1234567890000]");

	Json arrForCat(JsonType::Array);
	EXPECT_EQ(arrForCat.size(), 0);	
	arrForCat.add(2);
	arrForCat.add({12,13,14,15});
	Json subForCat(JsonType::Array);
	subForCat.add("math", 99);
	subForCat.add("str", 100);
	arrForCat.concat(subForCat);
	EXPECT_EQ(arrForCat.toString(), "[2,12,13,14,15,99,100]");

	EXPECT_EQ(subForCat.size(), 2);	
	EXPECT_EQ(arrForCat.size(), 7);	

	string objsArr = "[{\"name\":\"test1\",\"age\":1},{\"name\":\"test3\",\"age\":3},{\"name\":\"test4\",\"age\":5}]";
	Json oArr(objsArr);
	EXPECT_EQ(oArr.toString(), "[{\"name\":\"test1\",\"age\":1},{\"name\":\"test3\",\"age\":3},{\"name\":\"test4\",\"age\":5}]");

	Json arrForExt(JsonType::Array);
	arrForExt.add({2,3,5,6,8,9});
	arrForExt.push_front(1);
	EXPECT_EQ(arrForExt.toString(), "[1,2,3,5,6,8,9]");
	arrForExt.push_back(10);
	EXPECT_EQ(arrForExt.toString(), "[1,2,3,5,6,8,9,10]");
	arrForExt.push_front(Json({{"first",0},{"location",0}}));
	EXPECT_EQ(arrForExt.toString(), "[{\"first\":0,\"location\":0},1,2,3,5,6,8,9,10]");
	arrForExt.push_back(Json({{"last",11},{"location",12}}));
	EXPECT_EQ(arrForExt.toString(), "[{\"first\":0,\"location\":0},1,2,3,5,6,8,9,10,{\"last\":11,\"location\":12}]");

	arrForExt.clear();
	arrForExt.add({2,3,5,6,8,9});
	arrForExt.insert(2, 4);
	EXPECT_EQ(arrForExt.toString(), "[2,3,4,5,6,8,9]");
	Json ret = arrForExt.insert(-8, 7);
	EXPECT_EQ(ret.toString(), "[2,3,4,5,6,8,9]");
	arrForExt.insert(-2, 7);
	EXPECT_EQ(arrForExt.toString(), "[2,3,4,5,6,7,8,9]");

	Json rs(JsonType::Array);
	Json one;
	sub.clear();
	sub.concat("1");
	sub.concat("2");
	one.add("first", sub);
	rs.add(one);

	one.clear();
	sub.clear();
	sub.concat("3");
	sub.concat("4");
	one.add("second", sub);
	rs.add(one);
	EXPECT_EQ(rs.toString(), "[{\"first\":[\"1\",\"2\"]},{\"second\":[\"3\",\"4\"]}]");

	Json arrForRm("[1,2,3,4,5,6,7,8,9]");
	arrForRm.remove(3).removeFirst().removeLast();
	EXPECT_EQ(arrForRm.toString(), "[2,3,5,6,7,8]");
	arrForRm.remove(0).pop().pop_front().pop_back();
	EXPECT_EQ(arrForRm.toString(), "[5,6]");

	Json arr(JsonType::Array);
	EXPECT_EQ(arr.getValueType(), "Array");

	arr.add(1).add(true).add(89.9023).add(9876543210123456)		//max number length
		.add("This is a string value.").add(nullptr).add(Json({{"first","n01"}, {"second", "n02"}}));
	EXPECT_EQ(arr.toString(), 
			"[1,true,89.9023,9876543210123456,\"This is a string value.\",null,{\"first\":\"n01\",\"second\":\"n02\"}]");

	arr = Json("[    \"JSON Test Pattern pass1\",    {\"object with 1 member\":[\"array with 1 element\"]},    {},    [],    -42]");
	EXPECT_EQ(arr.toString(), "[\"JSON Test Pattern pass1\",{\"object with 1 member\":[\"array with 1 element\"]},{},[],-42]");

	arr = Json("[12345.6789e-7,12345.6789e-3,12345.6789e2,12345.6789e8]");      /***  only six significant decimals  ***/
	EXPECT_EQ(arr.toString(), "[0.001235,12.345679,1234567.89,1234567890000]");

	Json obj{ {"number", 1}, {"string", "a string"}, {"sub obj","{\"first\":\"one\",\"second\":\"two\"}"} };
	arr.clear().add({ 1,nullptr,true,"another string" }).concat(obj);
	EXPECT_EQ(arr.toString(), "[1,null,true,\"another string\",1,\"a string\",{\"first\":\"one\",\"second\":\"two\"}]");

	Json arr2(JsonType::Array);
	arr2.add({"one", "two", "three"});
	arr.concat(arr2);
	EXPECT_EQ(arr.toString(), "[1,null,true,\"another string\",1,\"a string\",{\"first\":\"one\",\"second\":\"two\"},\"one\",\"two\",\"three\"]");
	EXPECT_EQ(arr.size(), 10);

	rs = arr[0];
	EXPECT_EQ(rs.getValueType(), "Number");
	EXPECT_EQ(rs.toDouble(), 1);

	rs = arr[1];
	EXPECT_EQ(rs.getValueType(), "Null");
	EXPECT_EQ(rs.isNull(), true);

	rs = arr[2];
	EXPECT_EQ(rs.getValueType(), "True");
	EXPECT_EQ(rs.toBool(), true);

	rs = arr[6];
	EXPECT_EQ(rs.getValueType(), "Object");
	EXPECT_EQ(rs.toString(), "{\"first\":\"one\",\"second\":\"two\"}");

	rs = arr[6]["second"];
	EXPECT_EQ(rs.getValueType(), "String");
	EXPECT_EQ(rs.toString(), "two");

	rs = arr[10];
	EXPECT_EQ(rs.getValueType(), "Error");

	rs = arr.take(6);
	EXPECT_EQ(rs["first"].toString(), "one");
	EXPECT_EQ(arr.toString(), "[1,null,true,\"another string\",1,\"a string\",\"one\",\"two\",\"three\"]");

	rs = arr.slice(2, 6);			//get 2,3,4,5
	EXPECT_EQ(rs.toString(), "[true,\"another string\",1,\"a string\"]");
	rs = arr.slice(6);				// get 6-> last
	EXPECT_EQ(rs.toString(), "[\"one\",\"two\",\"three\"]");

	rs = arr.takes(2, 6);			//take 2,3,4,5
	EXPECT_EQ(rs.toString(), "[true,\"another string\",1,\"a string\"]");
	EXPECT_EQ(arr.toString(), "[1,null,\"one\",\"two\",\"three\"]");

	EXPECT_EQ(arr.first().toDouble(), 1);
	EXPECT_EQ(arr.last().toString(), "three");

	arr.insert(1, "insert a element");
	EXPECT_EQ(arr.toString(), "[1,\"insert a element\",null,\"one\",\"two\",\"three\"]");

	arr.insert(11, "Out of index range!");		//do nothing, when index out of index range
	EXPECT_EQ(arr.toString(), "[1,\"insert a element\",null,\"one\",\"two\",\"three\"]");

	arr.clear().push(5);
	EXPECT_EQ(arr.toString(), "[5]");
	arr.push_back(6);
	EXPECT_EQ(arr.toString(), "[5,6]");
	arr.push_front(4);
	EXPECT_EQ(arr.toString(), "[4,5,6]");
	arr.push_front(Json({ {"num1",1},{"num2",2} }));
	EXPECT_EQ(arr.toString(), "[{\"num1\":1,\"num2\":2},4,5,6]");
	arr.push_back(Json("[7,8,9]"));
	EXPECT_EQ(arr.toString(), "[{\"num1\":1,\"num2\":2},4,5,6,[7,8,9]]");

	arr.pop();
	EXPECT_EQ(arr.toString(), "[{\"num1\":1,\"num2\":2},4,5,6]");
	arr.pop_front();
	EXPECT_EQ(arr.toString(), "[4,5,6]");
	arr.pop_front();
	EXPECT_EQ(arr.toString(), "[5,6]");
	arr.pop_back();
	EXPECT_EQ(arr.toString(), "[5]");
	arr.pop_back();
	EXPECT_EQ(arr.isEmpty(), true);

}