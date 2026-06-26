#include "tim_it.h"
#include "usbd_cdc_if.h"

// 定时器中断回调函数
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  // 1ms定时器，判断USB接收完成
  if (htim->Instance == TIM6)
  {
    USB_Rx_Timeout_Check();
  }
}

