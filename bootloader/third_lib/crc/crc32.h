#ifndef __CRC32_H__
#define __CRC32_H__

#include <stdint.h>

/* 计算指定原始字节的 CRC32，不追加填充字节 */
uint32_t crc32(const uint8_t *buf, uint32_t length);

#endif
