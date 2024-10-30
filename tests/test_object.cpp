#include <cmath>
#include "gtest/gtest.h"
#include "../src/zjson.hpp"
#include <array>
#include <vector>

using namespace ZJSON;

TEST(TestObject, test_object_1) {
	Json dot;
	dot.add("l", 0.00059908820549026132);
	EXPECT_EQ(dot["l"].toString(), "0.000599");
	Json ajson;
	std::string data = "kevin";
	float f = 9.012345;
	long l = 123;
	long long ll = 56789;
	Json sub;
	sub.add("first", "this is the first.");
	Json subb;
	subb.add("sub2-1", "the second sub object.").add("sub2-2", "the second field.");
	sub.add("second obj", subb).add("a number", 666);

	ajson.add("long", l).add("longlong", ll).add("sex", true);
	ajson.add("name", data).add("school-cn", "第八十五中学").add("subObjct", sub);

	ajson.add("school-en", "the 85th.").add("age", 10).add("scores", 95.98);
	ajson.add("classroom", f).add("index", '6').add("nullkey", nullptr);

	EXPECT_EQ(ajson.toString(), "{\"long\":123,\"longlong\":56789,\"sex\":true,\"name\":\"kevin\",\"school-cn\":\"第八十五中学\",\"subObjct\":{\"first\":\"this is the first.\",\"second obj\":{\"sub2-1\":\"the second sub object.\",\"sub2-2\":\"the second field.\"},\"a number\":666},\"school-en\":\"the 85th.\",\"age\":10,\"scores\":95.98,\"classroom\":9.012345,\"index\":54,\"nullkey\":null}");

	EXPECT_DOUBLE_EQ(ajson["scores"].toDouble(), 95.98);
	EXPECT_EQ(ajson["name"].toString(), "kevin");
	EXPECT_EQ(ajson["age"].toDouble(), 10);
	EXPECT_TRUE(ajson["sex"].toBool());
	EXPECT_TRUE(ajson["nullkey"].isNull());
	EXPECT_EQ(ajson["age"].toDouble(), 10);
	EXPECT_EQ(ajson["subObjct"].toString(), "{\"first\":\"this is the first.\",\"second obj\":{\"sub2-1\":\"the second sub object.\",\"sub2-2\":\"the second field.\"},\"a number\":666}");

	string o3str = "{    \"JSON Test Pattern pass3\": {        \"The outermost value\": \"must be an object or array.\",        \"In this test\": \"It is an object.\"    }}";
	Json obj3Str(o3str);
	EXPECT_EQ(obj3Str.toString(), "{\"JSON Test Pattern pass3\":{\"The outermost value\":\"must be an object or array.\",\"In this test\":\"It is an object.\"}}");

	Json strobj(" a  \r\n{\"id\":\"a2b3c4d5\",\"name\":\"test001\",\"age\":19,\"score\":69.15}");
	EXPECT_EQ(strobj.getValueType(), "String");
	Json strObj("    \r\n{\"id\":\"a2b3c4d5\",\"name\":\"test001\",\"age\":19,\"score\":69.15}");
	EXPECT_EQ(strObj.getValueType(), "Object");

	Json subArray(JsonType::Array);
	subArray.add("I'm the first one.");
	subArray.add(2);
	Json sub2{{"math", 99},{"str", "a string."}};
	subArray.add(sub2);
	Json initializerObj{{"fkey", false},{"strkey","ffffff"},{"nkey", nullptr}, {"n1", 2}, {"num2", 9.98}, {"okey", subArray}};
	EXPECT_EQ(initializerObj.toString(), "{\"fkey\":false,\"strkey\":\"ffffff\",\"nkey\":null,\"n1\":2,\"num2\":9.98,\"okey\":[\"I'm the first one.\",2,{\"math\":99,\"str\":\"a string.\"}]}");

	Json sub11;
	sub11.add("yuwen", 88);
	sub11.add("hx", 90);
	Json sub12;
	sub12.add("math", 99);
	sub12.add("str", "a string.");
	Json sub13;
	sub13.add("music", 95);
	sub12.add("sub3", sub13);
	sub11.extend(sub12);
	EXPECT_EQ(sub11.toString(),"{\"yuwen\":88,\"hx\":90,\"math\":99,\"str\":\"a string.\",\"sub3\":{\"music\":95}}");

	Json sub20;
	sub20.add("yuwen", 66);
	sub20.add("draw", 11);
	sub20.add("hx", 90);
	Json sub21(JsonType::Array);
	sub21.add("math", 99);
	sub21.add("str", 100);
	Json sub22;
	sub22.add("music", 95);
	sub22.add("draw", 88);
	sub20.add("sub3", sub22);
	sub20.add("arr", sub21);
	sub20.remove("yuwen");
	EXPECT_EQ(sub20.toString(),"{\"draw\":11,\"hx\":90,\"sub3\":{\"music\":95,\"draw\":88},\"arr\":[99,100]}");

	sub20.add("yuwen", 66);
	sub20.remove("hx");
	EXPECT_EQ(sub20.toString(),"{\"draw\":11,\"sub3\":{\"music\":95,\"draw\":88},\"arr\":[99,100],\"yuwen\":66}");

	sub20.add("hx", 90);
	sub20.remove("sub3");
	EXPECT_EQ(sub20.toString(),"{\"draw\":11,\"arr\":[99,100],\"yuwen\":66,\"hx\":90}");

	sub20.add("sub3", sub22);
	sub20.remove("arr");
	EXPECT_EQ(sub20.toString(),"{\"draw\":11,\"yuwen\":66,\"hx\":90,\"sub3\":{\"music\":95,\"draw\":88}}");

	sub20.add("arr", sub21);
	sub20.remove("music");
	EXPECT_EQ(sub20.toString(),"{\"draw\":11,\"yuwen\":66,\"hx\":90,\"sub3\":{\"draw\":88},\"arr\":[99,100]}");

	sub20.remove("sub3");
	sub20.add("sub3", sub22);
	sub20.remove("draw");
	EXPECT_EQ(sub20.toString(),"{\"yuwen\":66,\"hx\":90,\"arr\":[99,100],\"sub3\":{\"music\":95}}");

	Json arr(JsonType::Array);
	Json sub31;
	sub31.add("yuwen", 66);
	sub31.add("draw", 11);
	sub31.add("hx", 90);
	arr.push_back(sub31);
	Json sub32(JsonType::Array);
	sub32.add("math", 99);
	sub32.add("str", 100);
	arr.push_back(sub32);
	Json sub33;
	sub33.add("music", 95);
	sub33.add("draw", 88);
	arr.push_back(sub33);

	Json rs;
	rs.add("data", arr);
	EXPECT_EQ(rs.toString(),"{\"data\":[{\"yuwen\":66,\"draw\":11,\"hx\":90},[99,100],{\"music\":95,\"draw\":88}]}");

	EXPECT_EQ(rs.size(), -1);	

	EXPECT_TRUE(sub31.contains("hx"));
	EXPECT_FALSE(sub31.contains("hhxx"));

	EXPECT_EQ(sub31.take("yuwen").toInt(), 66);
	EXPECT_EQ(sub31.toString(), "{\"draw\":11,\"hx\":90}");

	Json allKeys = sub31.getAllKeys();
	std::string allKeyString = "";
	int len = allKeys.size();
	for(int i = 0; i < len; i++)
		allKeyString.append(allKeys[i].toString());
	EXPECT_EQ(allKeyString, "drawhx");

	Json jsEmpty("{}");
	EXPECT_EQ(jsEmpty.toString(), "{}");

	jsEmpty = Json();
	EXPECT_EQ(jsEmpty.toString(), "{}");

	Json emptyKeys = jsEmpty.getAllKeys();
	len = emptyKeys.size();
	EXPECT_EQ(emptyKeys.size(), 0);

	std::string szData = "{\"start\":{\"x\":1,\"y\":2},\"end\":{\"x\":30,\"y\":25}}";
	Json jsData(szData);
	EXPECT_EQ(jsData["end"]["x"].toInt(), 30);
	EXPECT_EQ(jsData["start"].toString(), "{\"x\":1,\"y\":2}");

	Json eObj;
	eObj.add("eObj1", 1.23456789e39).add("eObj2", -9.87654321E120).add("little", -0.123456);
	EXPECT_EQ(eObj.toString(), "{\"eObj1\":1.234568e+39,\"eObj2\":-9.876543e+120,\"little\":-0.123456}");
	EXPECT_EQ(eObj["eObj1"].toDouble(), 1.23456789e+39);
	EXPECT_EQ(eObj["eObj2"].toDouble(), -9.87654321E120);

	Json fnull;
	fnull.add("nan-float", std::numeric_limits<float>::quiet_NaN()).add("nan-double", std::numeric_limits<double>::quiet_NaN());
	EXPECT_TRUE(fnull["nan-float"].isNull());
	EXPECT_TRUE(fnull["nan-double"].isNull());

	Json littleNumber;
	littleNumber.add("n1", 0.00070952).add("n2", 2.9999997615814209).add("n3", 0.00000123);
	EXPECT_EQ(littleNumber["n1"].toString(), "0.000709");
	EXPECT_EQ(littleNumber["n2"].toString(), "2.999999");
	EXPECT_EQ(littleNumber["n3"].toString(), "0.000001");
}