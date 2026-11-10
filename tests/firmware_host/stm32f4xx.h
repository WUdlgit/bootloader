#ifndef TEST_STM32F4XX_H
#define TEST_STM32F4XX_H
#include <stdint.h>
typedef enum {DISABLE, ENABLE} FunctionalState;
typedef struct {uint32_t ACR;} FLASH_TypeDef;
extern FLASH_TypeDef test_flash;
#define FLASH (&test_flash)
#define FLASH_ACR_DCEN 0x400U
#define FLASH_ACR_ICEN 0x200U
#define __DSB() ((void)0)
#define __ISB() ((void)0)
#endif
