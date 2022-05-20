#include <iostream>
#include "zjson.hpp"
#include <array>
#include <vector>

using namespace ZJSON;

int main(int argc, char* argv[])
{
	Json ajson(JsonType::Object);
	std::string data = "kevin";
	float f = 9.01234567;
	long l = 123;
	long long ll = 56789;
	ajson.AddValueBase("long", l);
	/*ajson.AddValueBase("longlong", ll);
	ajson.AddValueBase("sex", true);
	ajson.AddValueBase("name", data);
	ajson.AddValueBase("school-cn", "第八十五中学");
	ajson.AddValueBase("school-en", "the 85th.");
	ajson.AddValueBase("age", 10);
	ajson.AddValueBase("scores", 95.98);
	ajson.AddValueBase("classroom", f);
	ajson.AddValueBase("index", '6');
	ajson.AddValueBase("nullkey", nullptr);*/

	Json sub;

	ajson.AddValueJson("subJson", sub);

	Json subArray(JsonType::Array);

	ajson.AddValueJson("array", subArray);

	std::cout << std::endl;
	std::cout << ajson.toString() << std::endl;
}