#include <algorithm> 
#include <cctype>
#include <locale>
#include <vector>

namespace StrUtils {
	void ltrim(std::string& s);
	void rtrim(std::string& s);
	void trim(std::string& s);
	void toLowercase(std::string& str);
	std::vector<std::string> splitString(const std::string& str, const std::string& delimiter);
}