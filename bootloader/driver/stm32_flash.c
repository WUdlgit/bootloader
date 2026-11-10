#include <stddef.h>
#include "stm32_flash.h"
#include "stm32f4xx.h"
#include "stm32f4xx_flash.h"

/* 擦写后清理缓存，保证后续读回取到更新后的 Flash 内容 */
static void reset_flash_cache(void)
{
    uint32_t cache = FLASH->ACR & (FLASH_ACR_DCEN | FLASH_ACR_ICEN);

    FLASH_DataCacheCmd(DISABLE);
    FLASH_InstructionCacheCmd(DISABLE);
    FLASH_DataCacheReset();
    FLASH_InstructionCacheReset();
    if (cache & FLASH_ACR_DCEN)
        FLASH_DataCacheCmd(ENABLE);
    if (cache & FLASH_ACR_ICEN)
        FLASH_InstructionCacheCmd(ENABLE);
    __DSB();
    __ISB();
}

static void clear_flash_error(void)
{
    FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_OPERR | FLASH_FLAG_WRPERR |
                    FLASH_FLAG_PGAERR | FLASH_FLAG_PGPERR | FLASH_FLAG_PGSERR);
}

/* 本项目固定分区：先擦描述头并确认失效，再擦需要的 App 扇区 */
bool stm32_flash_erase(uint32_t length)
{
    if (length < 8 || length > APP_MAX_LENGTH)
        return false;

    FLASH_Unlock();
    clear_flash_error();

    /* 先擦除描述信息并读回确认，再擦 App，防止旧描述被继续使用 */
    FLASH_Status status = FLASH_EraseSector(FLASH_Sector_3, VoltageRange_3);
    reset_flash_cache();
    if (status != FLASH_COMPLETE || *(const uint32_t *)INFO_ADDRESS != 0xFFFFFFFFU)
    {
        FLASH_Lock();
        return false;
    }

    static const uint32_t sectors[] = {FLASH_Sector_4, FLASH_Sector_5,
                                       FLASH_Sector_6, FLASH_Sector_7};
    static const uint32_t offsets[] = {0, 64U * 1024U, 192U * 1024U, 320U * 1024U};
    for (uint32_t i = 0; i < 4; i++)
    {
        if (length <= offsets[i]) break;
        status = FLASH_EraseSector(sectors[i], VoltageRange_3);
        if (status != FLASH_COMPLETE) break;
    }

    FLASH_Lock();
    reset_flash_cache();
    return status == FLASH_COMPLETE;
}

/* 只向 App 区写入一块数据；按字节写入可直接处理非整字尾块 */
bool stm32_flash_program(uint32_t address, const uint8_t *buf, uint32_t block_length)
{
    if (buf == NULL || block_length == 0 || address < APP_ADDRESS ||
        address >= APP_END_ADDRESS || block_length > APP_END_ADDRESS - address)
        return false;

    FLASH_Unlock();
    clear_flash_error();
    FLASH_Status status = FLASH_COMPLETE;
    uint32_t offset = 0;
    while (offset < block_length)
    {
        if (((address + offset) & 3U) == 0 && block_length - offset >= 4U)
        {
            uint32_t word = (uint32_t)buf[offset] |
                            ((uint32_t)buf[offset + 1] << 8) |
                            ((uint32_t)buf[offset + 2] << 16) |
                            ((uint32_t)buf[offset + 3] << 24);
            status = FLASH_ProgramWord(address + offset, word);
            offset += 4;
        }
        else
        {
            status = FLASH_ProgramByte(address + offset, buf[offset]);
            offset++;
        }
        if (status != FLASH_COMPLETE)
            break;
    }
    FLASH_Lock();
    reset_flash_cache();
    if (status != FLASH_COMPLETE)
        return false;

    for (uint32_t i = 0; i < block_length; i++)
    {
        if (*(const uint8_t *)(address + i) != buf[i])
            return false;
    }
    return true;
}

/* 描述头固定为四个对齐的 32 位字段，由上层决定写入顺序 */
bool stm32_flash_program_word(uint32_t address, uint32_t data)
{
    if (address < INFO_ADDRESS || address > INFO_ADDRESS + 12U || (address & 3U))
        return false;

    FLASH_Unlock();
    clear_flash_error();
    FLASH_Status status = FLASH_ProgramWord(address, data);
    FLASH_Lock();
    reset_flash_cache();

    return status == FLASH_COMPLETE && *(const uint32_t *)address == data;
}
