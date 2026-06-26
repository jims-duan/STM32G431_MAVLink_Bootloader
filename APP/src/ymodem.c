#include "ymodem.h"
#include <string.h>
#include <stdlib.h>
#include "usbd_cdc_if.h"
#include "iap.h"
#include "crc.h"
#include "iwdg.h"

// 一页大小
static uint8_t YmodemBuff[FLASH_PAGE_SIZE];

YMODEM_State_Machine ymodem_fsm;

void YMODEM_Init(YMODEM_State_Machine* ymodem)
{
    ymodem->state = YMODEM_STATE_IDLE;           // 空闲状态
    ymodem->tick = 0;                             // 状态机计数0
    memset(ymodem->pack_buf, 0, sizeof(ymodem->pack_buf));            // 清空包缓冲区
    ymodem->pack_buf_offset = 0;                  // 包缓冲区偏移
    ymodem->header = 0;                          // 清空帧头
    ymodem->block_number = 0;                    // 当前块号
    ymodem->block_number_inv = 0;                // 包号反码
    ymodem->expected_block = 0;                  // 期望包号从0开始
    memset(ymodem->payload, 0, sizeof(ymodem->payload));
    memset(ymodem->filename, 0, sizeof(ymodem->filename));
    ymodem->file_size = 0;                        // 文件大小
    ymodem->received_size = 0;                    // 接收计数
    ymodem->flash_address = FLASH_APP_ADDR;        // FLASH初始地址
    ymodem->app_crc = 0;                          // 程序CRC值
}

void YMODEM_Reset(YMODEM_State_Machine* ymodem)
{
    YMODEM_Init(ymodem);
}


#define YMODEM_TX_TIMEOUT_MS  10
void YMODEM_SendByte(uint8_t data)
{
    uint32_t start_tick = HAL_GetTick();
    
    // 等待上一次发送完成，带超时
    while (usbd_type.usb_tx_complete == 0)
    {
        if ((HAL_GetTick() - start_tick) >= YMODEM_TX_TIMEOUT_MS)
        {
            // 超时处理
            usbd_type.usb_tx_complete = 1;  // 强制标记为完成
            return;
        }
    }
    usbd_type.usb_tx_complete = 0;
    CDC_Transmit_FS(&data, 1);
}

#define ISVALIDDEC(c)   ((c >= '0') && (c <= '9'))
#define CONVERTDEC(c)   (c - '0')
static uint32_t Str2Int(uint8_t* inputstr, uint32_t* intnum)
{
    uint32_t i = 0, res = 0;
    uint32_t val = 0;

    /* max 10-digit decimal input */
    for (i = 0; i < 11; i++)
    {
        if (inputstr[i] == '\0')
        {
            *intnum = val;
            /* return 1 */
            res = 1;
            break;
        }
        else if (ISVALIDDEC(inputstr[i]))
        {
            val = val * 10 + CONVERTDEC(inputstr[i]);
        }
        else
        {
            /* return 0, Invalid input */
            res = 0;
            break;
        }
    }
    /* Over 10 digit decimal --invalid */
    if (i >= 11)
    {
        res = 0;
    }
    return res;
}
static uint16_t CRC16_CCITT(const uint8_t* data, uint16_t len)
{
    uint16_t crc = 0x0000;
    uint16_t poly = 0x1021;

    for (uint16_t i = 0; i < len; i++)
    {
        crc ^= (data[i] << 8);
        for (uint8_t j = 0; j < 8; j++)
        {
            if (crc & 0x8000)
            {
                crc = (crc << 1) ^ poly;
            }
            else
            {
                crc <<= 1;
            }
        }
    }

    return crc;
}

void YMODEM_Parse(YMODEM_State_Machine *ymodem, uint8_t *data, uint16_t *len)
{
  if(*len < 1)
  {
    return;
  }
 
  uint8_t buff[1029] = {0};
  memcpy(buff, data, *len);        // 创建副本
  memset(data, 0, *len);     // 清除原数据
  *len = 0;
 
  ymodem->header            = buff[0];    // 头
  ymodem->block_number      = buff[1];    // 包号
  ymodem->block_number_inv  = buff[2];    // 包号反码
 
  if (ymodem->header != YMODEM_EOT)
  {
    // 包号验证
    if((ymodem->block_number ^ ymodem->block_number_inv) != 0xFF)
    {
      YMODEM_SendByte(YMODEM_NAK);
      return;
    }
   
    // 判断期望包号
    if((uint8_t)(ymodem->expected_block % 256) != ymodem->block_number)
    {
      YMODEM_SendByte(YMODEM_NAK);
      return;
    }
  }
 
 
  if(ymodem->state == YMODEM_STATE_COMPLETE)  // 传输结束
  {
    YMODEM_SendByte(YMODEM_ACK);
   
    ymodem->state = YMODEM_STATE_IDLE;

    // 验证失败直接复位重新进入bootloader
    if (iap_load_app(0x08008000) < 0)
    {
      __DSB();
      __ISB();
      NVIC_SystemReset();
    }
    return;
  }
 
  switch(ymodem->header)
  {
    case(YMODEM_SOH):       // 128字节数据包
    {
      // 计算 CRC
      uint16_t calc_crc = CRC16_CCITT(&buff[3], 128);
      // uint16_t calc_crc = (uint16_t)HAL_CRC_Calculate(&hcrc, (uint32_t*)&buff[3], 128);
      uint16_t recv_crc = (buff[131] << 8) | buff[132];

      // 校验 CRC
      if(calc_crc != recv_crc)
      {
        YMODEM_SendByte(YMODEM_NAK);

        return;
      }

      memcpy(ymodem->payload, &buff[3], 128); // 复制载荷
     
      if(ymodem->block_number == 0x00 && ymodem->expected_block == 0)  // 包ID为0
      {
        strcpy((char*)ymodem->filename, (char*)ymodem->payload);  // 复制文件名
        uint8_t *p = (ymodem->payload + strlen((char*)ymodem->filename) + 1); // 指针偏移
        uint8_t size_str[11] = {0};  // 最大4GB
        uint8_t i = 0;
        while (*p != ' ' && *p != '\0' && i < 10)
        {
            size_str[i++] = *p++;
        }
        size_str[i] = '\0';
        Str2Int(size_str, &ymodem->file_size); // 转换文件大小
        if (ymodem->file_size > 94 * 1024)
        {
          YMODEM_SendByte(YMODEM_CAN);
          YMODEM_SendByte(YMODEM_CAN);
          YMODEM_SendByte(YMODEM_CAN);
          YMODEM_SendByte(YMODEM_CAN);
          YMODEM_SendByte(YMODEM_CAN);
          __DSB();
          __ISB();
          NVIC_SystemReset();
        }
       
        YMODEM_SendByte(YMODEM_ACK);
        YMODEM_SendByte(YMODEM_C);
       
        ymodem->expected_block = ymodem->block_number + 1;
        ymodem->state = YMODEM_STATE_START;

        __HAL_CRC_DR_RESET(&hcrc);
      }
      else  // 正常数据包
      {
        static uint32_t n = 0;
        
        // 将数据追加到缓冲区
        memcpy(YmodemBuff + n, ymodem->payload, 128);
        n += 128;
        ymodem->received_size += 128;

        // 检查是否是最后一个数据包
        if (ymodem->received_size >= ymodem->file_size)
        {
          // 计算有效数据长度
          uint32_t valid_len = ymodem->file_size - (ymodem->received_size - 128);
          ymodem->received_size = ymodem->received_size - 128 + valid_len;
          
          // 原来最后128字节替换为 valid_len 字节
          n = n - 128 + valid_len;
          
          // 写入缓冲区中的所有数据（可能不足一页）
          if (n > 0)
          {
            ymodem->app_crc = HAL_CRC_Accumulate(&hcrc, (uint32_t*)YmodemBuff, n);
            iap_write_appbin(ymodem->flash_address, YmodemBuff, n);
            // 写入APP大小和CRC校验值
            uint8_t buffer[8];
            *(uint32_t*)(buffer + 0) = ymodem->file_size;
            *(uint32_t*)(buffer + 4) = ymodem->app_crc;
            FlashWriteMulti_AppendSafe(APP_INFO_ADDR, buffer, 8);
            ymodem->flash_address += n;
            n = 0;
          }
          
          ymodem->state = YMODEM_STATE_WAIT_EOT;
          YMODEM_SendByte(YMODEM_ACK);
          ymodem->expected_block = ymodem->block_number + 1;
          break;
        }
        
        // 缓冲区满（2KB），写入整页
        if (n >= FLASH_PAGE_SIZE)
        {
          n = 0;

          ymodem->app_crc = HAL_CRC_Accumulate(&hcrc, (uint32_t*)YmodemBuff, FLASH_PAGE_SIZE);
          iap_write_appbin(ymodem->flash_address, YmodemBuff, FLASH_PAGE_SIZE);
          ymodem->flash_address += FLASH_PAGE_SIZE;
        }
        
        YMODEM_SendByte(YMODEM_ACK);
        ymodem->expected_block = ymodem->block_number + 1;
      }
    }
    break;
    case(YMODEM_EOT):
    {
      static uint8_t i=0;
     
      i++;
      // if(i == 1)
      // {
      //   YMODEM_SendByte(YMODEM_NAK);
      // }
      // else if(i == 2)
      {
        YMODEM_SendByte(YMODEM_ACK);
        YMODEM_SendByte(YMODEM_C);
       
        ymodem->state = YMODEM_STATE_COMPLETE;
        ymodem->expected_block = 0; // 将接收最后一个包

        i = 0;
      }
    }
    break;
  }
}


