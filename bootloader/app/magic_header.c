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
app_check_result_t magic_header_validate_checked(app_check_detail_t *detail)
{
    const magic_header_t *info = (const magic_header_t *)INFO_ADDRESS;
    if (detail != NULL)
        *detail = (app_check_detail_t){0};
    if (!header_is_valid(info))
        return APP_CHECK_HEADER_VERIFY;
    if (!app_vector_is_valid(info->length))
        return APP_CHECK_VECTOR;
    uint32_t actual = crc32((const uint8_t *)APP_ADDRESS, info->length);
    if (detail != NULL)
    {
        detail->expected_crc = info->crc;
        detail->actual_crc = actual;
    }
    return actual == info->crc ? APP_CHECK_OK : APP_CHECK_IMAGE_CRC;
}

bool magic_header_validate(void)
{
    return magic_header_validate_checked(NULL) == APP_CHECK_OK;
}

app_check_result_t magic_header_save_checked(uint32_t length, uint32_t crc,
                                             app_check_detail_t *detail)
{
    if (detail != NULL)
        *detail = (app_check_detail_t){0};
    if (length < 8 || length > APP_MAX_LENGTH)
        return APP_CHECK_LENGTH;
    if (!app_vector_is_valid(length))
        return APP_CHECK_VECTOR;
    uint32_t actual = crc32((const uint8_t *)APP_ADDRESS, length);
    if (detail != NULL)
    {
        detail->expected_crc = crc;
        detail->actual_crc = actual;
    }
    if (actual != crc)
        return APP_CHECK_IMAGE_CRC;

    magic_header_t info = {MAGIC_HEADER_MAGIC, length, crc, 0};
    info.info_crc = crc32((const uint8_t *)&info, offsetof(magic_header_t, info_crc));
    const uint32_t offsets[] = {4, 8, 12, 0};
    const uint32_t values[] = {info.length, info.crc, info.info_crc, info.magic};
    for (uint32_t i = 0; i < 4; i++)
    {
        if (!stm32_flash_program_word(INFO_ADDRESS + offsets[i], values[i]))
        {
            if (detail != NULL)
                detail->field_offset = offsets[i];
            return APP_CHECK_HEADER_WRITE;
        }
    }
    return header_is_valid((const magic_header_t *)INFO_ADDRESS) ?
           APP_CHECK_OK : APP_CHECK_HEADER_VERIFY;
}

bool magic_header_save(uint32_t length, uint32_t crc)
{
    return magic_header_save_checked(length, crc, NULL) == APP_CHECK_OK;
}
