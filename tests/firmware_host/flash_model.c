#include "stm32f4xx_flash.h"

FLASH_TypeDef test_flash;
static int locked = 1;
static int fail_after = -1;
static int drop_after = -1;
static uint32_t erased;
static uint32_t bad_access;
static uint32_t image_crc_calls;

/* 统计实际 App CRC 调用，验证提交不重复扫描，上电检查仍重新扫描。 */
uint32_t __real_crc32(const uint8_t *buf, uint32_t length);
uint32_t __wrap_crc32(const uint8_t *buf, uint32_t length)
{
    if ((uint32_t)buf == 0x08010000)
        image_crc_calls++;
    return __real_crc32(buf, length);
}

uint32_t test_image_crc_calls(void) { return image_crc_calls; }

void test_reset(void)
{
    for (uint32_t i = 0x08000000; i < 0x08080000; i++)
        *(uint8_t *)i = 0xFF;
    *(uint32_t *)0x08000000 = 0x12345678;
    *(uint32_t *)0x08008000 = 0x87654321;
    locked = 1;
    fail_after = -1;
    drop_after = -1;
    erased = 0;
    bad_access = 0;
    image_crc_calls = 0;
    test_flash.ACR = FLASH_ACR_DCEN | FLASH_ACR_ICEN;
}

void test_fail_after(int count) { fail_after = count; }
void test_drop_after(int count) { drop_after = count; }
int test_locked(void) { return locked; }
uint32_t test_erased(void) { return erased; }
uint32_t test_bad_access(void) { return bad_access; }
void FLASH_Unlock(void) { locked = 0; }
void FLASH_Lock(void) { locked = 1; }
void FLASH_ClearFlag(uint32_t flags) { (void)flags; }

static int operation_fails(void)
{
    if (locked || fail_after == 0)
        return 1;
    if (fail_after > 0)
        fail_after--;
    return 0;
}

static int operation_drops(void)
{
    if (drop_after == 0)
        return 1;
    if (drop_after > 0)
        drop_after--;
    return 0;
}

FLASH_Status FLASH_EraseSector(uint32_t sector, uint8_t voltage)
{
    uint32_t address, length;
    if (sector == 3) { address = 0x0800C000; length = 16384; }
    else if (sector == 4) { address = 0x08010000; length = 65536; }
    else if (sector == 5) { address = 0x08020000; length = 131072; }
    else if (sector == 6) { address = 0x08040000; length = 131072; }
    else if (sector == 7) { address = 0x08060000; length = 131072; }
    else { bad_access++; return FLASH_ERROR_PROGRAM; }
    if (voltage != VoltageRange_3 || operation_fails())
        return FLASH_ERROR_PROGRAM;
    erased |= 1U << sector;
    if (!operation_drops())
    {
        for (uint32_t i = 0; i < length; i++)
            *(uint8_t *)(address + i) = 0xFF;
    }
    return FLASH_COMPLETE;
}

static int allowed(uint32_t address, uint32_t length)
{
    return (address >= 0x0800C000 && address <= 0x08010000 - length) ||
           (address >= 0x08010000 && address <= 0x08080000 - length);
}

FLASH_Status FLASH_ProgramByte(uint32_t address, uint8_t data)
{
    if (!allowed(address, 1)) { bad_access++; return FLASH_ERROR_PROGRAM; }
    if (operation_fails())
        return FLASH_ERROR_PROGRAM;
    if (!operation_drops())
        *(uint8_t *)address &= data;
    return FLASH_COMPLETE;
}

FLASH_Status FLASH_ProgramWord(uint32_t address, uint32_t data)
{
    if ((address & 3) || !allowed(address, 4)) { bad_access++; return FLASH_ERROR_PROGRAM; }
    if (operation_fails())
        return FLASH_ERROR_PROGRAM;
    if (!operation_drops())
        *(uint32_t *)address &= data;
    return FLASH_COMPLETE;
}

void FLASH_DataCacheCmd(FunctionalState state)
{
    if (state) test_flash.ACR |= FLASH_ACR_DCEN;
    else test_flash.ACR &= ~FLASH_ACR_DCEN;
}
void FLASH_InstructionCacheCmd(FunctionalState state)
{
    if (state) test_flash.ACR |= FLASH_ACR_ICEN;
    else test_flash.ACR &= ~FLASH_ACR_ICEN;
}
void FLASH_DataCacheReset(void) {}
void FLASH_InstructionCacheReset(void) {}
