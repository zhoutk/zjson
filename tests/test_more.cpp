#include <cmath>
#include "gtest/gtest.h"
#include "../src/zjson.hpp"

using namespace ZJSON;

TEST(TestMore, test_more_1) {
	Json config;
	bool loaded = false;
	const char* candidates[] = {
		"tests/more.json",
		"../tests/more.json",
		"../../tests/more.json",
		"../../../tests/more.json",
		"../../../../tests/more.json"
	};
	for (const char* p : candidates) {
		config = Json::FromFile(p);
		if (!config.isError()) {
			loaded = true;
			break;
		}
	}
	EXPECT_TRUE(loaded);
	config.remove("gps");
	//std::cout << config.toString();
	EXPECT_EQ(config.toString(), "{\"radar\":[{\"route_name\":\"Radar\"},{\"route_name\":\"R2\"}]}");

	Json toStr;
	Json arr(JsonType::Array);
	toStr.add("arr", arr);
	toStr.add("num", 1);
	EXPECT_EQ(toStr.toString(), "{\"arr\":[],\"num\":1}");
}