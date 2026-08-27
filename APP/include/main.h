#ifndef MAIN_H
#define MAIN_H

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef  DEBUG
    #define DBG(format,...)  printf(format, ##__VA_ARGS__)  // mjt 2026-01-29 add
    //打印16进制
	#define DBG_HEX(buff, len) do{\
								for(int i = 0; i < len; i++)\
								{ printf("%02x ", buff[i]); }\
								printf("\n");\
								}while(0);
	//打印字符
	#define DBG_CHAR(buff, len) do{\
								for(int i = 0; i < len; i++)\
								{ printf("%c", buff[i]); }\
								printf("\n");\
								}while(0);
#else
    #define DBG(format,...)  
	#define DBG_HEX(buff, len)
	#define DBG_CHAR(buff, len)
#endif

// 转16进制函数
#define HEX2CHAR(x) ((x) < 10 ? ('0' + (x)) : ('A' + (x) - 10))


#ifdef __cplusplus
}
#endif

#endif
