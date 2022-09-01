#include <cmath>
#include "gtest/gtest.h"
#include "../src/zjson.hpp"
#include <array>
#include <vector>

using namespace ZJSON;

TEST(TestObject, test_object_1) {
	Json ajson;
	std::string data = "kevin";
	float f = 9.01234567;
	long l = 123;
	long long ll = 56789;
	Json sub;
	EXPECT_EQ(sub.addSubitem("first", "this is the first."), true);
	Json subb;
	EXPECT_EQ(subb.addSubitem("sub2-1", "the second sub object."), true);
	EXPECT_EQ(subb.addSubitem("sub2-2", "the second field."), true);
	EXPECT_EQ(sub.addSubitem("second obj", subb), true);
	EXPECT_EQ(sub.addSubitem("a number", 666), true);

	EXPECT_EQ(ajson.addSubitem("long", l), true);
	EXPECT_EQ(ajson.addSubitem("longlong", ll), true);
	EXPECT_EQ(ajson.addSubitem("sex", true), true);
	EXPECT_EQ(ajson.addSubitem("name", data), true);
	EXPECT_EQ(ajson.addSubitem("school-cn", "第八十五中学"), true);

	EXPECT_TRUE(ajson.addSubitem("subObjct", sub));

	EXPECT_EQ(ajson.addSubitem("school-en", "the 85th."), true);
	EXPECT_EQ(ajson.addSubitem("age", 10), true);
	EXPECT_EQ(ajson.addSubitem("scores", 95.98), true);
	EXPECT_EQ(ajson.addSubitem("classroom", f), true);
	EXPECT_EQ(ajson.addSubitem("index", '6'), true);
	EXPECT_EQ(ajson.addSubitem("nullkey", nullptr), true);

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

	Json subArray(JsonType::Array);
	subArray.addSubitem("I'm the first one.");
	subArray.addSubitem(2);
	Json sub2{{"math", 99},{"str", "a string."}};
	subArray.addSubitem(sub2);
	Json initializerObj{{"fkey", false},{"strkey","ffffff"},{"nkey", nullptr}, {"n1", 2}, {"num2", 9.98}, {"okey", subArray}};
	EXPECT_EQ(initializerObj.toString(), "{\"fkey\":false,\"strkey\":\"ffffff\",\"nkey\":null,\"n1\":2,\"num2\":9.98,\"okey\":[\"I'm the first one.\",2,{\"math\":99,\"str\":\"a string.\"}]}");

	Json sub11;
	sub11.addSubitem("yuwen", 88);
	sub11.addSubitem("hx", 90);
	Json sub12;
	sub12.addSubitem("math", 99);
	sub12.addSubitem("str", "a string.");
	Json sub13;
	sub13.addSubitem("music", 95);
	sub12.addSubitem("sub3", sub13);
	sub11.extend(sub12);
	EXPECT_EQ(sub11.toString(),"{\"yuwen\":88,\"hx\":90,\"math\":99,\"str\":\"a string.\",\"sub3\":{\"music\":95}}");
}