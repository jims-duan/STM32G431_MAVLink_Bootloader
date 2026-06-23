#ifndef _YMODEM_H
#define _YMODEM_H

#include "main.h"
#include "iap.h"

typedef enum
{
    YMODEM_SOH = 0x01,  /** 起始块：128字节数据包 */
    YMODEM_STX = 0x02,  /** 起始块：1024字节数据包 */
    YMODEM_EOT = 0x04,  /** 传输结束标志 */
    YMODEM_ACK = 0x06,  /** 确认应答（正确接收） */
    YMODEM_NAK = 0x15,  /** 否认应答（接收错误，请求重发） */
    YMODEM_CAN = 0x18,  /** 取消传输（连续发送两个中止会话） */
    YMODEM_C = 0x43,  /** 请求CRC16校验模式（ASCII字符'C'） */
} YMODEM_ControlChar;

typedef enum
{
    YMODEM_STATE_IDLE,            // 空闲状态
    YMODEM_STATE_START,           // 开始传输
    YMODEM_STATE_RECEIVE_SOH,     // 接收SOH包
    YMODEM_STATE_WAIT_EOT,        // 等待结束
    YMODEM_STATE_COMPLETE,        // 传输结束
}
YMODEM_STATE;

typedef struct
{
    YMODEM_STATE state;         // 状态
    uint32_t tick;              // 状态机计数器
    uint8_t pack_buf[1029];     // 包缓冲区
    uint16_t pack_buf_offset;   // 包缓冲区偏移
    uint8_t header;             // 帧头
    uint8_t block_number;       // 当前块号（0-255）
    uint8_t block_number_inv;   // 包号反码
    uint32_t expected_block;    // 期望包号(用取余计算)，方便操作
    uint8_t payload[1024];      // 有效载荷
    uint8_t filename[128];      // 文件名
    uint32_t file_size;         // 文件大小(字节)
    uint32_t received_size;     // 接收计数

    uint32_t flash_address;     // 程序存储地址
    uint32_t app_crc;           // 程序CRC值
}
YMODEM_State_Machine;
extern YMODEM_State_Machine ymodem_fsm;

static inline uint32_t get_tick_diff(uint32_t current, uint32_t previous)
{
    return current - previous;
}

void YMODEM_Init(YMODEM_State_Machine* ymodem);
void YMODEM_SendByte(uint8_t data);
void YMODEM_Parse(YMODEM_State_Machine *ymodem, uint8_t *data, volatile uint16_t *len);


#endif

