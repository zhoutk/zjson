#include <cmath>
#include "gtest/gtest.h"
#include "../src/zjson.hpp"
#include <array>
#include <vector>

using namespace ZJSON;

TEST(TestArray, test_array_1) {
	Json ajson(JsonType::Array);
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
	EXPECT_EQ(ajson.AddValueBase("nullkey", nullptr), true);

	EXPECT_EQ(ajson.toString(), "[123,56789,true,\"kevin\",\"第八十五中学\",\"the 85th.\",10,95.98,9.012345,54,null]");
}