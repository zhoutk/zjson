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
	EXPECT_EQ(sub.AddSubitem("first", "this is the first."), true);
	Json subb;
	EXPECT_EQ(subb.AddSubitem("sub2-1", "the second sub object."), true);
	EXPECT_EQ(subb.AddSubitem("sub2-2", "the second field."), true);
	EXPECT_EQ(sub.AddSubitem("second obj", subb), true);
	EXPECT_EQ(sub.AddSubitem("a number", 666), true);

	EXPECT_EQ(ajson.AddSubitem("long", l), true);
	EXPECT_EQ(ajson.AddSubitem("longlong", ll), true);
	EXPECT_EQ(ajson.AddSubitem("sex", true), true);
	EXPECT_EQ(ajson.AddSubitem("name", data), true);
	EXPECT_EQ(ajson.AddSubitem("school-cn", "第八十五中学"), true);

	EXPECT_TRUE(ajson.AddSubitem("subObjct", sub));

	EXPECT_EQ(ajson.AddSubitem("school-en", "the 85th."), true);
	EXPECT_EQ(ajson.AddSubitem("age", 10), true);
	EXPECT_EQ(ajson.AddSubitem("scores", 95.98), true);
	EXPECT_EQ(ajson.AddSubitem("classroom", f), true);
	EXPECT_EQ(ajson.AddSubitem("index", '6'), true);
	EXPECT_EQ(ajson.AddSubitem("nullkey", nullptr), true);

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
	subArray.AddSubitem("I'm the first one.");
	subArray.AddSubitem(2);
	Json sub2{{"math", 99},{"str", "a string."}};
	subArray.AddSubitem(sub2);
	Json initializerObj{{"fkey", false},{"strkey","ffffff"},{"nkey", nullptr}, {"n1", 2}, {"num2", 9.98}, {"okey", subArray}};
	EXPECT_EQ(initializerObj.toString(), "{\"fkey\":false,\"strkey\":\"ffffff\",\"nkey\":null,\"n1\":2,\"num2\":9.98,\"okey\":[\"I'm the first one.\",2,{\"math\":99,\"str\":\"a string.\"}]}");
}