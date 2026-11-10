#ifndef __STM32_FLASH_H__
#define __STM32_FLASH_H__

#include <stdbool.h>
#include <stdint.h>

/* 固定分区，结束地址不包含在对应区域内 */
#define APP_ADDRESS       0x08010000U
#define APP_END_ADDRESS   0x08080000U
#define APP_MAX_LENGTH    (APP_END_ADDRESS - APP_ADDRESS)
#define INFO_ADDRESS      0x0800C000U

/* 开始升级时擦除描述头和本次固件需要的 App 扇区 */
bool stm32_flash_erase(uint32_t length);
/* 向 App 区写入数据并逐字节读回比较 */
bool stm32_flash_program(uint32_t address, const uint8_t *buf, uint32_t block_length);
/* 向描述头区域写入一个对齐的 32 位字段 */
bool stm32_flash_program_word(uint32_t address, uint32_t data);

#endif
