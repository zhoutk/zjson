#include <iostream>
#include "zjson.hpp"
#include <chrono>
#include <limits>

using namespace ZJSON;

const int LEN = 1000;

time_t GetCurrentTimeMsec() {
	auto time = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::system_clock::now());
	time_t timestamp = time.time_since_epoch().count();
	return timestamp;
}

int main(int argc, char* argv[])
{
	time_t bg = GetCurrentTimeMsec();
	Json rs;
	rs.add("code", 200);
	rs.add("std-null-double", std::numeric_limits<double>::quiet_NaN());
	//Json data(JsonType::Array);
	//for (int i = 0; i < LEN; i++) {
	//	Json aline(JsonType::Array);
	//	for (int j = 0; j < LEN; j++) {
	//		double anumber = rand() / 1.2345 * 321.567;
	//		aline.push(anumber);
	//	}
	//	data.push(std::move(aline));
	//}
	//rs.add("data", std::move(data));
	//time_t t1 = GetCurrentTimeMsec();
	string ourStr = rs.toString();
	//time_t t2 = GetCurrentTimeMsec();
	std::cout << ourStr << std::endl;
	//std::cout << " --- generate json using time count : " << t1 - bg << std::endl;
	//std::cout << " --- json toString using time count : " << t2 - t1 << std::endl;
	//std::cout << " --- show json string using time count : " << GetCurrentTimeMsec() - t2 << std::endl;
}