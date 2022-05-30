#include <cmath>
#include "gtest/gtest.h"
#include "../src/zjson.hpp"
#include <array>
#include <vector>

using namespace ZJSON;

TEST(TestParse, test_parse_1) {
	string strBase = "{\"age\":10,\"array\":[\"first\",null],\"true\":true,\"subobj\":{\"field01\":\"obj01\",\"subNumber\":99,\"null\":null}}"; 
	EXPECT_EQ(Json(strBase).toString(), "{\"age\":10,\"array\":[\"first\",null],\"true\":true,\"subobj\":{\"field01\":\"obj01\",\"subNumber\":99,\"null\":null}}");
	
	string strThreeLevel = "{\"array\":[\"first\",90.12387,null,2,true],\"true\":true,\"false\":false,\"null\":null,\"age\":18,\"score\":12.3456,\"name\":\"kevin\"}";
	EXPECT_EQ(Json(strThreeLevel).toString(),"{\"array\":[\"first\",90.12387,null,2,true],\"true\":true,\"false\":false,\"null\":null,\"age\":18,\"score\":12.3456,\"name\":\"kevin\"}");
}