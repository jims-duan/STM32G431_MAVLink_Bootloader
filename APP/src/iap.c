#include "iap.h"
#include "stmflash.h"

iapfun jump2app; 
uint16_t iapbuf[2048];   
//appxaddr:应用程序的起始地址
//appbuf:应用程序CODE.
//appsize:应用程序大小(字节).
void iap_write_appbin(uint32_t appxaddr,uint8_t *appbuf,uint32_t appsize)
{
	uint16_t t;
	uint16_t i=0;
	uint16_t temp;
	uint32_t fwaddr=appxaddr;//当前写入的地址
	uint8_t *dfu=appbuf;
	for(t=0;t<appsize;t+=2)
	{						    
		temp=(uint16_t)dfu[1]<<8;
		temp+=(uint16_t)dfu[0];	  
		dfu+=2;//偏移2个字节
		iapbuf[i++]=temp;	    
		if(i==1024)
		{
			i=0;
			FlashWriteMulti_AppendSafe(fwaddr,iapbuf,1024);	
			fwaddr+=2048;//偏移2048  16=2*8.所以要乘以2.
		}
	}
	if(i)FlashWriteMulti_AppendSafe(fwaddr,iapbuf,i);//将最后的一些内容字节写进去.  
}

//跳转到应用程序段
//appxaddr:用户代码起始地址.
int iap_load_app(uint32_t appxaddr)
{
    // 验证栈指针
    uint32_t msp_value = *(volatile uint32_t*)appxaddr;
    uint32_t reset_vector = *(volatile uint32_t*)(appxaddr + 4);
    
    // STM32G431 SRAM: 0x20000000 - 0x20007FFF (32KB)
	// 栈顶指针有效范围: 0x20000004 到 0x20008000 (包含边界值)
    if (msp_value < 0x20000000 || msp_value > 0x20008000)
	{
        return -1;
    }
    
    // 检查栈指针是否对齐（必须是8字节对齐）
    if ((msp_value & 0x07) != 0)
	{
        return -2;
    }
    
    // 检查复位向量是否在Flash范围内
    if (reset_vector < FLASH_BASE || reset_vector > FLASH_BASE + FLASH_SIZE)
	{
        return -3;
    }
    
    // 检查复位向量是否可执行（Thumb模式，地址最低位为1）
    if ((reset_vector & 1) == 0)
	{
        return -4;
    }
    
    // 关闭所有中断
    __disable_irq();
    
	HAL_DeInit(); // 关闭SysTick等系统节拍
	__HAL_RCC_USB_FORCE_RESET();
	__HAL_RCC_USB_RELEASE_RESET();
	__HAL_RCC_USB_CLK_DISABLE();

    // 禁用所有外设中断
    for(int i = 0; i < 8; i++) {
        NVIC->ICER[i] = 0xFFFFFFFF;
        NVIC->ICPR[i] = 0xFFFFFFFF;
    }

    __DSB();
    __ISB();

    // 设置栈指针并跳转
    __set_MSP(msp_value);
    jump2app=(iapfun)*(uint32_t*)(appxaddr+4);
    jump2app();
}

