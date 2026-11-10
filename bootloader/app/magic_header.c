#include <stddef.h>
#include "magic_header.h"
#include "stm32_flash.h"
#include "crc32.h"

static bool app_vector_is_valid(uint32_t length)
{
    uint32_t app_stack = *(const uint32_t *)APP_ADDRESS;
    uint32_t app_entry = *(const uint32_t *)(APP_ADDRESS + 4);
    uint32_t entry_address = app_entry & ~1U;

    /* 当前 App 的栈放在 128 KiB SRAM，栈顶允许等于 RAM 上边界 */
    if (app_stack <= 0x20000000U || app_stack > 0x20020000U || (app_stack & 7U))
        return false;
    if (!(app_entry & 1U) || entry_address < APP_ADDRESS + 8 ||
        entry_address - APP_ADDRESS > length - 2)
        return false;
    return true;
}

/* 只检查描述头，保存完成后不再重复扫描 App */
static bool header_is_valid(const magic_header_t *info)
{
    if (info->magic != MAGIC_HEADER_MAGIC || info->length < 8 || info->length > APP_MAX_LENGTH)
        return false;
    return crc32((const uint8_t *)info, offsetof(magic_header_t, info_crc)) == info->info_crc;
}

/* 上电和启动命令必须同时检查描述头、向量及整包固件 */
bool magic_header_validate(void)
{
    const magic_header_t *info = (const magic_header_t *)INFO_ADDRESS;

    if (!header_is_valid(info) || !app_vector_is_valid(info->length))
        return false;
    return crc32((const uint8_t *)APP_ADDRESS, info->length) == info->crc;
}

bool magic_header_save(uint32_t length, uint32_t crc)
{
    if (length < 8 || length > APP_MAX_LENGTH || !app_vector_is_valid(length))
        return false;
    /* 提交描述头前，在这里统一计算一次整包 CRC */
    if (crc32((const uint8_t *)APP_ADDRESS, length) != crc)
        return false;

    magic_header_t info = {MAGIC_HEADER_MAGIC, length, crc, 0};
    info.info_crc = crc32((const uint8_t *)&info, offsetof(magic_header_t, info_crc));

    /* 先写长度和两份校验值，最后写 magic，半途失败不会留下有效描述头 */
    if (!stm32_flash_program_word(INFO_ADDRESS + 4, info.length) ||
        !stm32_flash_program_word(INFO_ADDRESS + 8, info.crc) ||
        !stm32_flash_program_word(INFO_ADDRESS + 12, info.info_crc) ||
        !stm32_flash_program_word(INFO_ADDRESS, info.magic))
        return false;

    return header_is_valid((const magic_header_t *)INFO_ADDRESS);
}
