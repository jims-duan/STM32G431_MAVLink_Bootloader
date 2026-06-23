#ifndef _IAP_H
#define _IAP_H

#include "stmflash.h"

typedef  int (*iapfun)(void);				//定义一个函数类型的参数.

#define FLASH_APP_ADDR		0x08008000  	//第一个应用程序起始地址(存放在FLASH)
											//保留0X08000000~0X0800FFFF的空间为IAP使用
#define APP_INFO_ADDR		0x0801F800		// 最后2KB保存程序长度和CRC等信息

void iap_write_appbin(uint32_t appxaddr,uint8_t *appbuf,uint32_t appsize);
int iap_load_app(uint32_t appxaddr);


#endif
