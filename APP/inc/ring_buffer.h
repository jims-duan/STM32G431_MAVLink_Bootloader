#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#define RING_BUFFER_SIZE 4096
typedef struct
{
    uint8_t buffer[RING_BUFFER_SIZE];
    uint16_t head;
    uint16_t tail;
    uint16_t size;
}
RingBuffer_Structure;
extern RingBuffer_Structure usb_ringBuffer;

// 函数声明
void RingBuff_Init(RingBuffer_Structure* ring);
bool RingBuff_IsEmpty(RingBuffer_Structure* ring);
bool RingBuff_IsFull(RingBuffer_Structure* ring);
uint16_t RingBuff_GetSize(RingBuffer_Structure* ring);
uint16_t RingBuff_GetSpace(RingBuffer_Structure* ring);
bool RingBuff_WriteByte(RingBuffer_Structure* ring, uint8_t data);
bool RingBuff_ReadByte(RingBuffer_Structure* ring, uint8_t* data);
uint16_t RingBuff_WriteBytes(RingBuffer_Structure* ring, uint8_t* src, uint16_t len);
uint16_t RingBuff_ReadBytes(RingBuffer_Structure* ring, uint8_t* dst, uint16_t len);
void RingBuff_Clear(RingBuffer_Structure* ring);

#endif
