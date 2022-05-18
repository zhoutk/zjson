#include <iostream>
#include "zjson.hpp"
#include <array>
#include <vector>

using namespace ZJSON;

int main(int argc, char* argv[])
{
	Json ajson;
	std::string data = "kevin";
	float f = 9.01234567;
	ajson.AddValue("sex", true);
	ajson.AddValue("name", data);
	ajson.AddValue("school-cn", "第八十五中学");
	ajson.AddValue("school-en", "the 85th.");
	ajson.AddValue("age", 10);
	ajson.AddValue("scores", 95.98);
	ajson.AddValue("classroom", f);
	ajson.AddValue("index", '6');
	std::cout << ajson.toString() << std::endl;

	system("pause");
}