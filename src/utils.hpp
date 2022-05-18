#pragma once

#include <vector>

using std::string;

class Utils {
public:
	static bool stringContain(const string& str, const string& to) {
		return str.find(to) != string::npos;
	}

	static bool stringEqualTo(const string& str, const string& to) {
		return str.compare(to) == 0;
	}

	static bool stringEndWith(const string& str, const string& tail) {
		return str.compare(str.size() - tail.size(), tail.size(), tail) == 0;
	}

	static bool stringStartWith(const string& str, const string& head) {
		return str.compare(0, head.size(), head) == 0;
	}

	static std::vector<string> split(const string& strtem, const char a)
	{
		std::vector<string>res;
		string::size_type pos1, pos2;
		pos2 = strtem.find(a);
		pos1 = 0;
		while (string::npos != pos2)
		{
			res.push_back(strtem.substr(pos1, pos2 - pos1));
			pos1 = pos2 + 1;
			pos2 = strtem.find(a, pos1);
		}
		res.push_back(strtem.substr(pos1));
		return res;
	}
};