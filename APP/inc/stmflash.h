#ifndef __STMFLASH_H
#define __STMFLASH_H

#include "main.h"

#define FLASH_BASE_ADDR     0x08000000

uint8_t flash_erase_page(uint16_t start, uint8_t len, uint8_t bank);
uint8_t flash_read(uint32_t addr, uint8_t* data, uint32_t len_bytes);
uint8_t flash_write(uint32_t addr, uint8_t* data, uint32_t len_bytes);

void FlashWriteMulti(uint32_t addr, uint16_t* data, uint16_t halfword_count);
uint8_t FlashWriteMulti_AppendSafe(uint32_t addr, uint16_t* data, uint16_t halfword_count);


#endif
