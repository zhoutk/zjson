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
	float f = 9.01234567;
	ajson.AddValue("name", data);
	ajson.AddValue("school", "第八十五中学");
	ajson.AddValue("age", 18);
	ajson.AddValue("scores", 95.98);
	ajson.AddValue("classroom", f);
	ajson.AddValue("index", '6');
	std::cout << ajson.toString() << std::endl;

	system("pause");
}