#pragma once
#include <string>

using namespace std;

class pub
{
public:
	static std::string dec2hex(int i, int width);
	static string DecIntToHexStr(long long num);
	static string DecStrToHexStr(string str);
};

