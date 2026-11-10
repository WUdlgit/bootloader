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

/* 检查描述头及其对应的 App，供上电和启动命令共用 */
bool magic_header_validate(void);
/* 先校验固件，再保存并检查描述头；整包 CRC 只在提交前计算一次 */
bool magic_header_save(uint32_t length, uint32_t crc);

#endif
