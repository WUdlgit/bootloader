#include "crc32.h"

/* CRC-32/ISO-HDLC：初值和最终异或为全 1，与电脑端 zlib.crc32 一致 */
uint32_t crc32(const uint8_t *buf, uint32_t length)
{
    uint32_t crc = 0xFFFFFFFFU;

    for (uint32_t i = 0; i < length; i++)
    {
        crc ^= buf[i];
        for (uint32_t bit = 0; bit < 8; bit++)
        {
            if (crc & 1U)
                crc = (crc >> 1) ^ 0xEDB88320U;
            else
                crc >>= 1;
        }
    }
    return crc ^ 0xFFFFFFFFU;
}
