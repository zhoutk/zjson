#include <cmath>
#include "gtest/gtest.h"
#include "../src/zjson.hpp"

using namespace ZJSON;

TEST(TestMore, test_more_1) {
	Json config = Json::FromFile("more.json");
	config.remove("gps");
	//std::cout << config.toString();
	EXPECT_EQ(config.toString(), "{\"radar\":[{\"route_name\":\"Radar\"},{\"route_name\":\"R2\"}]}");

	Json toStr;
	Json arr(JsonType::Array);
	toStr.add("arr", arr);
	toStr.add("num", 1);
	EXPECT_EQ(toStr.toString(), "{\"arr\":[],\"num\":1}");
}