#ifndef __MAGIC_HEADER_H__
#define __MAGIC_HEADER_H__

#include <stdbool.h>
#include <stdint.h>
#include "stm32_flash.h"

#define MAGIC_HEADER_MAGIC 0x46575041U

/* 四个小端字段共 16 字节，与整理前的板内布局一致 */
typedef struct
{
    uint32_t magic;     // 有效标记，最后写入
    uint32_t length;    // 原始固件长度
    uint32_t crc;       // 固件内容的 CRC32
    uint32_t info_crc;  // 这个描述头前 12 字节的 CRC32
} magic_header_t;

typedef enum
{
    APP_CHECK_OK = 0,
    APP_CHECK_LENGTH,
    APP_CHECK_VECTOR,
    APP_CHECK_IMAGE_CRC,
    APP_CHECK_HEADER_WRITE,
    APP_CHECK_HEADER_VERIFY
} app_check_result_t;

typedef struct
{
    uint32_t expected_crc;
    uint32_t actual_crc;
    uint32_t field_offset;
} app_check_detail_t;

app_check_result_t magic_header_save_checked(uint32_t length, uint32_t crc,
                                             app_check_detail_t *detail);
app_check_result_t magic_header_validate_checked(app_check_detail_t *detail);

/* 检查描述头及其对应的 App，供上电和启动命令共用 */
bool magic_header_validate(void);
/* 先校验固件，再保存并检查描述头；整包 CRC 只在提交前计算一次 */
bool magic_header_save(uint32_t length, uint32_t crc);

#endif
