#include <iostream>
#include "zjson.hpp"
#include "check_type.hpp"
#include <array>
#include <vector>

using namespace ZJSON;

int main(int argc, char* argv[])
{
	Json ajson;
	ajson.AddValue("name", "kevin");
	ajson.AddValue("age", 18);
	std::cout << ajson.toString() << std::endl;

	system("pause");
}