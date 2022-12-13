#include <cmath>
#include "gtest/gtest.h"
#include "../src/zjson.hpp"
#include <array>
#include <vector>

using namespace ZJSON;

TEST(TestParse, test_parse_1) {
	string strBase = "{\"array\":[\"first\",null],\"true\":true,\"subobj\":{\"field01\":\"obj01\",\"subNumber\":99,\"null\":null},\"age\":10}"; 
	Json base2(strBase);
	EXPECT_EQ(base2["age"].toString(), "10");
	
	strBase = "{\"age\":10,\"array\":[\"first\",null],\"true\":true,\"subobj\":{\"field01\":\"obj01\",\"subNumber\":99,\"null\":null}}"; 
	Json base(strBase);
	EXPECT_EQ(base.toString(), "{\"age\":10,\"array\":[\"first\",null],\"true\":true,\"subobj\":{\"field01\":\"obj01\",\"subNumber\":99,\"null\":null}}");
	EXPECT_EQ(base["age"].toInt(), 10);
	EXPECT_EQ(base["array"][0].toString(), "first");
	EXPECT_TRUE(base["array"][1].isNull());
	EXPECT_TRUE(base["true"].isTrue());
	EXPECT_TRUE(base["subobj"]["null"].isNull());
	EXPECT_EQ(base["subobj"]["field01"].toString(), "obj01");


	string strMultiLevel = "{\"array\":[\"first\",90.12387,null,{\"three01\":0.1,\"three02\":[9,7,2],\"three03\":\"the end\"},2,true],\"true\":true,\"false\":false,\"null\":null,\"age\":18,\"score\":12.3456,\"name\":\"kevin\"}";
	Json multiLevel(strMultiLevel);
	EXPECT_EQ(multiLevel.toString(),"{\"array\":[\"first\",90.12387,null,{\"three01\":0.1,\"three02\":[9,7,2],\"three03\":\"the end\"},2,true],\"true\":true,\"false\":false,\"null\":null,\"age\":18,\"score\":12.3456,\"name\":\"kevin\"}");
	EXPECT_EQ(multiLevel["array"][0].toString(), "first");
	EXPECT_EQ(multiLevel["array"][1].toDouble(), 90.12387);
	EXPECT_EQ(multiLevel["array"][3]["three03"].toString(), "the end");
	EXPECT_EQ(multiLevel["array"][3]["three02"][2].toInt(), 2);

	Json pid{{"pid", 19692},{"age",12}};
	Json UserData {
		{"data", pid},
		{"type", 11024}
	};
	EXPECT_EQ(UserData["type"].toInt(), 11024);
	EXPECT_EQ(UserData["age"].toInt(), 12);
	EXPECT_EQ(UserData.toString(), "{\"data\":{\"pid\":19692,\"age\":12},\"type\":11024}");
}