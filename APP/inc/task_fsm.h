#ifndef _TASK_FSM_H
#define _TASK_FSM_H

#include "main.h"

#include "stmflash.h"
#include "usbd_cdc_if.h"
#include <stdarg.h>
#include "iap.h"
#include "usbd_cdc_if.h"
#include "ymodem.h"
#include "led.h"
#include "led_fsm.h"


#define UPDATE_MAGIC_APP  0xA55AA55A
extern volatile uint32_t g_UpdateFlag;

typedef enum
{
    FSM_STATE_IDLE,             // 空闲
    FSM_STATE_BOOT_OR_APP,      // 判断进入boot还是APP
    FSM_STATE_BOOT,             // BOOTLOADER升级
    FSM_STATE_APP,              // 启动APP
    FSM_STATE_YMODEM,           // YMODEM接收
} TaskFAM_State;

typedef struct
{
    TaskFAM_State state;                // 当前状态
    uint8_t (*get_key_state)(void);     // 获取按键状态
    uint32_t tick_500ms;                // 500ms周期
} Task_FSM_Type;
extern Task_FSM_Type task_fsm;

void tesk_fsm(Task_FSM_Type* fsm, uint32_t tick);



#endif
