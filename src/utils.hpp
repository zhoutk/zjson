#pragma once

#include <string>
#include <vector>
#if defined(__GNUC__)
#include <memory>       // std::unique_ptr
#include <cxxabi.h>     // abi::__cxa_demangle
#endif

namespace ZJSON {
	using std::string;

	#define GetTypeName(T) check_type<T>()
	#define GetVarTypeName(var) check_type<decltype(var)>()

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


	template <typename T>
	struct check
	{
		check(std::string& out)
		{
	#   if defined(__GNUC__)
			char* real_name = abi::__cxa_demangle(typeid(T).name(), nullptr, nullptr, nullptr);
			out = real_name;
			free(real_name);
	#   else
			out = typeid(T).name();
	#   endif
		}
	};


	template <typename T>
	inline std::string check_type(void)
	{
		std::string str;
		check<T> { str };
		return std::move(str);
	}

}