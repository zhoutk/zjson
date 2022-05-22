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
	EXPECT_EQ(sub.AddValueBase("first", "this is the first."), true);
	Json subb;
	EXPECT_EQ(subb.AddValueBase("sub2-1", "the second sub object."), true);
	EXPECT_EQ(subb.AddValueBase("sub2-2", "the second field."), true);
	EXPECT_EQ(sub.AddValueJson("second obj", subb), true);
	EXPECT_EQ(sub.AddValueBase("a number", 666), true);

	EXPECT_EQ(ajson.AddValueBase("long", l), true);
	EXPECT_EQ(ajson.AddValueBase("longlong", ll), true);
	EXPECT_EQ(ajson.AddValueBase("sex", true), true);
	EXPECT_EQ(ajson.AddValueBase("name", data), true);
	EXPECT_EQ(ajson.AddValueBase("school-cn", "第八十五中学"), true);

	EXPECT_TRUE(ajson.AddValueJson("subObjct", sub));

	EXPECT_EQ(ajson.AddValueBase("school-en", "the 85th."), true);
	EXPECT_EQ(ajson.AddValueBase("age", 10), true);
	EXPECT_EQ(ajson.AddValueBase("scores", 95.98), true);
	EXPECT_EQ(ajson.AddValueBase("classroom", f), true);
	EXPECT_EQ(ajson.AddValueBase("index", '6'), true);
	EXPECT_EQ(ajson.AddValueBase("nullkey", nullptr), true);

	EXPECT_EQ(ajson.toString(), "{\"long\":123,\"longlong\":56789,\"sex\":true,\"name\":\"kevin\",\"school-cn\":\"第八十五中学\",\"subObjct\":{\"first\":\"this is the first.\",\"second obj\":{\"sub2-1\":\"the second sub object.\",\"sub2-2\":\"the second field.\"},\"a number\":666},\"school-en\":\"the 85th.\",\"age\":10,\"scores\":95.98,\"classroom\":9.012345,\"index\":54,\"nullkey\":null");

	EXPECT_DOUBLE_EQ(ajson["scores"].toDouble(), 95.98);
	EXPECT_EQ(ajson["age"].toDouble(), 10);
	EXPECT_TRUE(ajson["sex"].toBool());
	EXPECT_TRUE(ajson["nullkey"].isNull());
	EXPECT_EQ(ajson["age"].toDouble(), 10);
	EXPECT_EQ(ajson["subObjct"].toString(), "{\"first\":\"this is the first.\",\"second obj\":{\"sub2-1\":\"the second sub object.\",\"sub2-2\":\"the second field.\"},\"a number\":666}");

}