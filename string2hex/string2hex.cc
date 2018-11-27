#include <stdio.h>
#include <iostream>
#include <stdlib.h>

//运用C strtoul库实现
void ww_parse_at_cmd(std::string at_cmd_str, unsigned char *at_cmd)
{
   //unsigned char cmd[15]={0};
   for(int i = 0; i < at_cmd_str.length(); i=i+2 , at_cmd++)
   {
     *at_cmd = strtoul((char *)at_cmd_str.substr(i,2).c_str(), 0, 16);
   }
}

//测试代码, 测试结果: 255 16 102
int main()
{
	unsigned char at_cmd[15]={0};

	std::string STR = "ff1066";
	ww_parse_at_cmd(STR, at_cmd);

	for (int i = 0; i < 3 ; i++)
	{
		printf("%d ", at_cmd[i]);
        }	
}
