#include "task_fsm.h"

// 放入.noinit段，上电不自动清零
__attribute__((section(".noinit"))) volatile uint32_t g_UpdateFlag;

uint8_t GetKey(void);

Task_FSM_Type task_fsm = 
{
    .state = FSM_STATE_IDLE,
    .get_key_state = GetKey,
    .tick_500ms = 0,

};

uint8_t GetKey(void)
{
    if (HAL_GPIO_ReadPin(key_GPIO_Port, key_Pin) == GPIO_PIN_RESET)
    {
        return 1;
    }
    return 0;
}

// 主任务状态机
void tesk_fsm(Task_FSM_Type* fsm, uint32_t tick)
{
    switch(fsm->state)
    {
        // 空闲
        case(FSM_STATE_IDLE):
        {
            // 关闭LED
            LED_FSM_SetOFFEvent(&led_fsm);

            fsm->state = FSM_STATE_BOOT_OR_APP;
        }
        break;
        // 判断进入boot还是APP
        case(FSM_STATE_BOOT_OR_APP):
        {
            if(GetKey() == 1)   // 引脚触发，强制进入固件升级
            {
                g_UpdateFlag = 0;
                fsm->state = FSM_STATE_BOOT;
            }
            else                    // 引脚不接
            {
                // 软件复位，且有更新标志
                if (__HAL_RCC_GET_FLAG(RCC_FLAG_SFTRST) != RESET
                    && g_UpdateFlag == UPDATE_MAGIC_APP)
                {
                    fsm->state = FSM_STATE_BOOT;
                }
                else if(g_UpdateFlag != UPDATE_MAGIC_APP)
                {
                    fsm->state = FSM_STATE_APP;
                }

                // 清除复位标志
                __HAL_RCC_CLEAR_RESET_FLAGS();
            }
        }
        break;
        // 进入APP
        case(FSM_STATE_APP):
        {
            g_UpdateFlag = 0;
            // 没有事件发生，直接跳转APP
            if (iap_load_app(0x08008000) < 0)
            {
                // 固件损坏或非固件文件，执行bootloader等待接收固件
                fsm->state = FSM_STATE_BOOT;
            }
        }
        break;
        // BOOTLOADER升级
        case(FSM_STATE_BOOT):
        {
            // BOOTLOADER升级
            LED_FSM_SetBreathingEvent(&led_fsm, 1000);

            if ((ymodem_fsm.state == YMODEM_STATE_IDLE) 
                && (get_tick_diff(tick, fsm->tick_500ms) >= 500))
            {
                fsm->tick_500ms = tick;

                if(usbd_type.BuffLen > 0)
                {
                    fsm->state = FSM_STATE_YMODEM;
                    LED_FSM_SetOFFEvent(&led_fsm);
                }
                if (usbd_type.usb_vcp_opened) // 等待上位机打开串口
                {
                    YMODEM_SendByte(YMODEM_C);
                }
            }
        }
        break;
        // YMODEM接收
        case(FSM_STATE_YMODEM):
        {
            // USB数据接收完成
            if (usbd_type.usb_rx_complete == 1)
            {
                usbd_type.usb_rx_complete = 0;  // 清除接收完成标志
                
                uint16_t len = usbd_type.BuffLen;
                usbd_type.BuffLen = 0;  // 清空数组长度
                // YMODEM解析(128协议)
                if (len == 133 || (usbd_type.ReceBuff[0] == 0x04)) // 标准包大小
                {
                    HAL_GPIO_TogglePin(led_GPIO_Port, led_Pin);
                    YMODEM_Parse(&ymodem_fsm, usbd_type.ReceBuff, &len);
                }
            }
        }
        break;
    }
}



