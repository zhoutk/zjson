#include <iostream>
#include "zjson.hpp"
#include "check_type.hpp"
#include <array>
#include <vector>

using namespace ZJSON;

int main(int argc, char* argv[])
{
	/*Json ajson;
	ajson.AddValueString("name", "kevin");
	ajson.AddValue(Type::Number, "age", 18);
	std::cout << ajson.toString() << std::endl;*/

	bool pp;
	int pp1;
	long pp2;
	long long pp3;
	float pp4;
	double pp5;
	string pp6;
	std::vector<int>pp7;
	std::array<int,8>pp8;

	std::cout << GetVarTypeName(pp) << std::endl;
	std::cout << GetVarTypeName(pp1) << std::endl;
	std::cout << GetVarTypeName(pp2) << std::endl;
	std::cout << GetVarTypeName(pp3) << std::endl;
	std::cout << GetVarTypeName(pp4) << std::endl;
	std::cout << GetVarTypeName(pp5) << std::endl;
	std::cout << GetVarTypeName(pp6) << std::endl;
	std::cout << GetVarTypeName(pp7) << std::endl;
	std::cout << GetVarTypeName(pp8) << std::endl;

	system("pause");
}