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
	EXPECT_EQ(ajson.AddValueBase("long", l), true);
	EXPECT_EQ(ajson.AddValueBase("longlong", ll), true);
	EXPECT_EQ(ajson.AddValueBase("sex", true), true);
	EXPECT_EQ(ajson.AddValueBase("name", data), true);
	EXPECT_EQ(ajson.AddValueBase("school-cn", "第八十五中学"), true);
	EXPECT_EQ(ajson.AddValueBase("school-en", "the 85th."), true);
	EXPECT_EQ(ajson.AddValueBase("age", 10), true);
	EXPECT_EQ(ajson.AddValueBase("scores", 95.98), true);
	EXPECT_EQ(ajson.AddValueBase("classroom", f), true);
	EXPECT_EQ(ajson.AddValueBase("index", '6'), true);

	EXPECT_EQ(ajson.toString(), "{ \"long\":123, \"longlong\" : 56789, \"sex\" : true, \"name\" : \"kevin\", \"school-cn\" : \"第八十五中学\", \"school-en\" : \"the 85th.\", \"age\" : 10, \"scores\" : 95.98, \"classroom\" : 9.012345, \"index\" : 54 }");
}