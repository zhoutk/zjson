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
	EXPECT_EQ(sub.AddSubitem("this is the first."), true);
	Json subb(JsonType::Array);
	EXPECT_EQ(subb.AddSubitem("the second sub object."), true);
	EXPECT_EQ(subb.AddSubitem("the second field."), true);
	EXPECT_EQ(sub.AddSubitem("second obj", subb), true);
	EXPECT_EQ(sub.AddSubitem("a number", 666), true);

	EXPECT_EQ(ajson.AddSubitem("long", l), true);
	EXPECT_EQ(ajson.AddSubitem("longlong", ll), true);
	EXPECT_EQ(ajson.AddSubitem("sex", true), true);
	EXPECT_EQ(ajson.AddSubitem("name", data), true);
	EXPECT_EQ(ajson.AddSubitem("school-cn", "第八十五中学"), true);

	EXPECT_TRUE(ajson.AddSubitem("subArray", sub));

	EXPECT_EQ(ajson.AddSubitem("school-en", "the 85th."), true);
	EXPECT_EQ(ajson.AddSubitem("age", 10), true);
	EXPECT_EQ(ajson.AddSubitem("scores", 95.98), true);
	EXPECT_EQ(ajson.AddSubitem("classroom", f), true);
	EXPECT_EQ(ajson.AddSubitem("index", '6'), true);
	EXPECT_EQ(ajson.AddSubitem("nullkey", nullptr), true);

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
}