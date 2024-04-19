#include "pub.h"
#include <sstream>

//iҪת����ʮ����������widthת����Ŀ�ȣ�λ��������0
std::string pub::dec2hex(int i, int width)
{
	std::stringstream ioss;     //�����ַ�����
	std::string s_temp;         //���ת�����ַ�
	ioss << std::hex << i;      //��ʮ������ʽ���
	ioss >> s_temp;

	if (width > s_temp.size())
	{
		std::string s_0(width - s_temp.size(), '0');      //λ��������0
		s_temp = s_0 + s_temp;                            //�ϲ�
	}

	std::string s = s_temp.substr(s_temp.length() - width, s_temp.length());    //ȡ��widthλ
	return s;
}

string pub::DecIntToHexStr(long long num)			//ʮ����������ʮ�������ַ���ת��
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


string pub::DecStrToHexStr(string str)			//ʮ�����ַ���ʮ�������ַ�ת��
{
	long long Dec = 0;
	for (int i = 0; i < str.size(); ++i)
		Dec = Dec * 10 + str[i] - '0';
	return DecIntToHexStr(Dec);
}
