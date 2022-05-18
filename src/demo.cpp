#include <iostream>
#include "zjson.hpp"
#include "check_type.hpp"
#include <array>
#include <vector>

using namespace ZJSON;

int main(int argc, char* argv[])
{
	Json ajson;
	std::string data = "kevin";
	ajson.AddValue("name", data);
	ajson.AddValue("school", "第八十五中学");
	ajson.AddValue("age", 18);
	std::cout << ajson.toString() << std::endl;

	system("pause");
}