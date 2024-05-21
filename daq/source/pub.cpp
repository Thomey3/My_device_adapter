#include "pub.h"
#include <sstream>

//i要转化的十进制整数，width转化后的宽度，位数不足则补0
std::string pub::dec2hex(int i, int width)
{
	std::stringstream ioss;     //定义字符串流
	std::string s_temp;         //存放转化后字符
	ioss << std::hex << i;      //以十六制形式输出
	ioss >> s_temp;

	if (width > s_temp.size())
	{
		std::string s_0(width - s_temp.size(), '0');      //位数不够则补0
		s_temp = s_0 + s_temp;                            //合并
	}

	std::string s = s_temp.substr(s_temp.length() - width, s_temp.length());    //取右width位
	return s;
}

string pub::DecIntToHexStr(long long num)			//十进制整数到十六进制字符的转换
{
	string str;
	long long Temp = num / 16;
	int left = num % 16;
	if (Temp > 0)
		str += DecIntToHexStr(Temp);
	if (left < 10)
		str += (left + '0');
	else
		str += ('A' + left - 10);
	return str;
}


string pub::DecStrToHexStr(string str)			//十进制字符到十六进制字符转换
{
	long long Dec = 0;
	for (int i = 0; i < str.size(); ++i)
		Dec = Dec * 10 + str[i] - '0';
	return DecIntToHexStr(Dec);
}
