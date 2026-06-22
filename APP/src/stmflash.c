#include "stmflash.h"
#include <string.h>

// 内部函数：按页删除FLASH数据
static uint8_t flash_erase_page_internal(uint16_t start, uint8_t len, uint8_t bank)
{
    uint32_t err;
    uint8_t ret;
    FLASH_EraseInitTypeDef EraseInitStruct;
    
    EraseInitStruct.TypeErase   = FLASH_TYPEERASE_PAGES;
    EraseInitStruct.Page        = start;
    EraseInitStruct.NbPages     = len;
    EraseInitStruct.Banks       = bank;
    
    HAL_FLASH_Unlock();
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_OPTVERR | FLASH_FLAG_PGSERR | FLASH_FLAG_WRPERR);
    
    for(uint8_t i = 0; i < 10; i++)
    {
        ret = HAL_FLASHEx_Erase(&EraseInitStruct, &err);
        if(ret == HAL_OK)
            break;
    }
    
    HAL_FLASH_Lock();
    return ret;
}

// 公开的擦除函数（如果需要单独擦除）
uint8_t flash_erase_page(uint16_t start, uint8_t len, uint8_t bank)
{
    return flash_erase_page_internal(start, len, bank);
}

// 读取FLASH数据
uint8_t flash_read(uint32_t addr, uint8_t* data, uint32_t len_bytes)
{
    // 读不需要强制8对齐，去掉原限制
    for(uint32_t i = 0; i < len_bytes; i++)
    {
        data[i] = *(__IO uint8_t *)(addr + i);
    }
    return 0;
}

// 原始覆盖写入：直接擦除页面，旧数据全部丢失（保留兼容旧代码）
void FlashWriteMulti(uint32_t addr, uint16_t* data, uint16_t halfword_count)
{
    uint32_t byte_count = halfword_count * 2;
    uint32_t start_page = (addr - FLASH_BASE_ADDR) / FLASH_PAGE_SIZE;
    uint32_t end_page = (addr + byte_count - 1 - FLASH_BASE_ADDR) / FLASH_PAGE_SIZE;
    uint32_t page_count = end_page - start_page + 1;
    
    // 先擦除
    flash_erase_page(start_page, page_count, 0);
    
    // 再写入
    flash_write(addr, (uint8_t*)data, byte_count);
}

// 底层双字写入（仅空白区写入，无擦除逻辑）
uint8_t flash_write(uint32_t addr, uint8_t* data, uint32_t len_bytes)
{
    uint8_t ret;
    uint64_t write_data;
    uint32_t offset = 0;
    uint32_t remaining = len_bytes;
    
    // 参数检查：G4必须8字节对齐写入
    if(addr & 0x07)
        return 255;
    if(len_bytes == 0)
        return HAL_OK;
    
    // 写入不做擦除，擦除交给上层Append函数处理
    HAL_FLASH_Unlock();
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_OPTVERR | FLASH_FLAG_PGSERR | FLASH_FLAG_WRPERR);
    
    while(remaining > 0)
    {
        write_data = 0xFFFFFFFFFFFFFFFFULL;
        for(uint8_t i = 0; i < 8 && offset + i < len_bytes; i++)
        {
            ((uint8_t*)&write_data)[i] = data[offset + i];
        }
        
        ret = HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, 
                                addr + offset, 
                                write_data);
        if(ret != HAL_OK)
        {
            HAL_FLASH_Lock();
            return ret;
        }
        
        offset += 8;
        remaining = (remaining > 8) ? remaining - 8 : 0;
    }
    
    HAL_FLASH_Lock();
    return HAL_OK;
}


static uint8_t g_page_cache[FLASH_PAGE_SIZE];
/**
 * @brief 安全追加/覆盖写入：自动读取页面原有数据，合并新数据后回写，不丢失旧数据
 * @param addr 写入起始地址（必须8字节对齐）
 * @param data 待写入半字数组
 * @param halfword_count 半字数量
 * @retval 0成功，非0失败码
 */
uint8_t FlashWriteMulti_AppendSafe(uint32_t addr, uint16_t* data, uint16_t halfword_count)
{
    const uint32_t PAGE_SIZE = FLASH_PAGE_SIZE;
    uint32_t byte_cnt = halfword_count * 2;
    uint32_t end_addr = addr + byte_cnt - 1;

    // 地址边界校验
    if((addr & 0x07) != 0)
        return 255;
    if(addr < FLASH_BASE_ADDR || end_addr > FLASH_BASE_ADDR + FLASH_SIZE - 1)
        return 255;

    // 计算涉及的起止页
    uint16_t start_pg = (addr - FLASH_BASE_ADDR) / PAGE_SIZE;
    uint16_t end_pg = (end_addr - FLASH_BASE_ADDR) / PAGE_SIZE;
    uint8_t pg_num = end_pg - start_pg + 1;

    // 静态缓存容量判断：超出最大支持页数直接返回失败
    if(pg_num > 1)
        return 255;

    uint32_t buf_total_size = pg_num * PAGE_SIZE;
    uint8_t *page_buf = g_page_cache;

    // 1. 读取所有页面原始数据到缓存
    uint32_t cur_page_addr = FLASH_BASE_ADDR + start_pg * PAGE_SIZE;
    uint32_t buf_offset = 0;
    for(uint16_t p = 0; p < pg_num; p++)
    {
        flash_read(cur_page_addr, page_buf + buf_offset, PAGE_SIZE);
        buf_offset += PAGE_SIZE;
        cur_page_addr += PAGE_SIZE;
    }

    // 2. 在缓存中覆盖/追加新数据
    uint32_t data_offset_in_buf = addr - (FLASH_BASE_ADDR + start_pg * PAGE_SIZE);
    memcpy(page_buf + data_offset_in_buf, data, byte_cnt);

    // 3. 擦除全部涉及页面
    uint8_t ret = flash_erase_page_internal(start_pg, pg_num, 0);
    if(ret != HAL_OK)
    {
        return ret;
    }

    // 4. 将合并后的数据写回Flash
    uint32_t write_start_addr = FLASH_BASE_ADDR + start_pg * PAGE_SIZE;
    ret = flash_write(write_start_addr, page_buf, buf_total_size);

    return ret;
}

