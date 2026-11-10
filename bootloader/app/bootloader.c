#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "bootloader.h"
#include "boot_entry.h"
#include "magic_header.h"
#include "stm32_flash.h"
#include "bl_usart.h"
#include "tim_delay.h"
#include "stm32f4xx.h"

static uint8_t rxbuf[1024];
static uint32_t firmware_length;  // 固件总长度
static uint32_t completed_length; // 累计成功写入的长度
static uint32_t received;         // 本块实际收到的字节数
static uint32_t total_received;   // 累计接收量
static uint32_t firmware_crc;

void app_handoff(uint32_t stack, uint32_t entry)
    __attribute__((noreturn));



/* 按换行接收长度和 CRC 头，正文使用另一条按长度接收的路径 */
static bool receive_line(char *buf, uint32_t capacity)
{
    uint32_t used = 0;
    uint8_t data;

    while (used < capacity - 1)
    {
        if (!bl_usart_read_byte(&data, 3000U) || data == '\0')
            return false;
        buf[used++] = (char)data;
        if (data == '\n')
        {
            buf[used] = '\0';
            return true;
        }
    }
    return false;
}

static bool parse_length(const char *line, uint32_t *out_length, uint32_t *out_crc)
{
    // SIZE=十进制长度 CRC=八位十六进制校验值，最后是 LF 或 CRLF
    if (strncmp(line, "SIZE=", 5) != 0 || line[5] < '0' || line[5] > '9')
        return false;

    char *end;
    errno = 0;
    unsigned long length = strtoul(line + 5, &end, 10);
    if (errno == ERANGE || length < 8 || length > APP_MAX_LENGTH ||
        strncmp(end, " CRC=", 5) != 0)
        return false;

    const char *start = end + 5;
    uint32_t crc = 0;
    for (uint32_t i = 0; i < 8; i++)
    {
        uint32_t digit;
        char data = start[i];
        if (data >= '0' && data <= '9')
            digit = data - '0';
        else if (data >= 'A' && data <= 'F')
            digit = data - 'A' + 10;
        else if (data >= 'a' && data <= 'f')
            digit = data - 'a' + 10;
        else
            return false;
        crc = (crc << 4) | digit;
    }

    if (strcmp(start + 8, "\n") != 0 && strcmp(start + 8, "\r\n") != 0)
        return false;
    *out_length = (uint32_t)length;
    *out_crc = crc;
    return true;
}

/* 本块长度已在调用前限制为 1～sizeof(rxbuf)，这里只负责收进固定缓冲区 */
static bool store_in_rxbuf(uint32_t length)
{
    received = 0;

    while (received < length)
    {
        if (!bl_usart_read_byte(&rxbuf[received], 3000U))
            return false;
        received++;
        total_received++;
    }
    return true;
}

static void discard_input(void)
{
    // 失败后等串口连续空闲 100 ms，正文残留不能当成 u/g 命令
    uint32_t start = tim_delay_get_tick();
    while ((uint32_t)(tim_delay_get_tick() - start) < 100U)
    {
        uint32_t status = USART3->SR;
        if (status & (USART_SR_RXNE | USART_SR_ORE | USART_SR_NE | USART_SR_FE | USART_SR_PE))
        {
            (void)USART3->DR;
            start = tim_delay_get_tick();
        }
    }
}

/* 一次升级：接收头、擦除、逐块写入、整包校验，最后保存描述头 */
static bool receive_firmware(void)
{
    char line[80];
    firmware_length = 0;
    completed_length = 0;
    total_received = 0;
    received = 0;

    bl_usart_printf("[BOOT] HEADER\r\n");
    if (!receive_line(line, sizeof(line)) ||
        !parse_length(line, &firmware_length, &firmware_crc))
    {
        bl_usart_printf("[BOOT] ERROR header\r\n");
        return false;
    }

    if (!stm32_flash_erase(firmware_length))
    {
        bl_usart_printf("[BOOT] ERROR erase\r\n");
        return false;
    }
    bl_usart_printf("[BOOT] DATA %lu %u\r\n",
                        (unsigned long)firmware_length, (unsigned int)sizeof(rxbuf));

    while (completed_length < firmware_length)
    {
        uint32_t block_length = firmware_length - completed_length;
        if (block_length > sizeof(rxbuf))
            block_length = sizeof(rxbuf);

        if (!store_in_rxbuf(block_length))
        {
            bl_usart_printf("[BOOT] ERROR receive %lu\r\n", (unsigned long)total_received);
            return false;
        }
        if (!stm32_flash_program(APP_ADDRESS + completed_length, rxbuf, block_length))
        {
            bl_usart_printf("[BOOT] ERROR write %lu\r\n", (unsigned long)completed_length);
            return false;
        }

        completed_length += block_length;
        bl_usart_printf("[BOOT] ACK %lu\r\n", (unsigned long)completed_length);
    }

    if (!magic_header_save(firmware_length, firmware_crc))
    {
        bl_usart_printf("[BOOT] ERROR verify_or_commit\r\n");
        return false;
    }
    bl_usart_printf("[BOOT] DONE %lu %08lX\r\n",
                        (unsigned long)completed_length, (unsigned long)firmware_crc);
    return true;
}

/* 启动前重新校验 App，再清理 Bootloader 使用的中断和串口 */
static void start_app(void)
{
    if (!magic_header_validate())
    {
        bl_usart_printf("[BOOT] ERROR invalid_app\r\n");
        return;
    }

    // 更新后重新读取向量，不能使用升级前保存的栈和入口
    uint32_t app_stack = *(const uint32_t *)APP_ADDRESS;
    uint32_t app_entry = *(const uint32_t *)(APP_ADDRESS + 4);
    bl_usart_printf("[BOOT] BOOTING\r\n");
    while (USART_GetFlagStatus(USART1, USART_FLAG_TC) == RESET);

    __disable_irq();
    SysTick->CTRL = 0;
    SysTick->LOAD = 0;
    SysTick->VAL = 0;
    SCB->ICSR = SCB_ICSR_PENDSTCLR_Msk | SCB_ICSR_PENDSVCLR_Msk;
    for (uint32_t i = 0; i < 3; i++)
    {
        NVIC->ICER[i] = 0xFFFFFFFFU;
        NVIC->ICPR[i] = 0xFFFFFFFFU;
    }
    USART_DeInit(USART3);
    USART_DeInit(USART1);
    SCB->VTOR = APP_ADDRESS;
    __DSB();
    __ISB();
    app_handoff(app_stack, app_entry);
}

void bootloader_main(void)
{
    boot_entry_init();
    bool requested = boot_entry_take_request();
    bool held = boot_entry_key_pressed();
    bool valid = magic_header_validate();
    int pending = -1;
    printf("[BOOT] single App serial IAP\r\n");
    printf("[BOOT] App %s\r\n", valid ? "valid" : "invalid");

    if (valid && !requested && !held)
    {
        bl_usart_printf("[BOOT] AUTOBOOT 1000\r\n");
        uint32_t start = tim_delay_get_tick();
        bool stay = false;
        while ((uint32_t)(tim_delay_get_tick() - start) < 1000U)
        {
            if (boot_entry_key_pressed())
            {
                stay = true;
                break;
            }
            if (USART_GetFlagStatus(USART3, USART_FLAG_RXNE) != RESET)
            {
                char data = (char)USART_ReceiveData(USART3);
                if (data == '?' || data == 'u' || data == 'g')
                {
                    pending = data;
                    stay = true;
                    break;
                }
            }
        }
        if (!stay)
            start_app();
    }
    printf("[BOOT] upgrade mode%s\r\n", requested ? " (App request)" : "");
    GPIO_ResetBits(GPIOB, GPIO_Pin_0);
    bl_usart_printf("[BOOT] READY\r\n");

    while (1)
    {
        char data;
        if (pending >= 0)
        {
            data = (char)pending;
            pending = -1;
        }
        else
        {
            if (USART_GetFlagStatus(USART3, USART_FLAG_RXNE) == RESET)
                continue;
            data = (char)USART_ReceiveData(USART3);
        }
        if (data == '?')
            bl_usart_printf("[BOOT] READY\r\n");
        else if (data == 'g')
            start_app();
        else if (data == 'u')
        {
            if (!receive_firmware())
                discard_input();
            bl_usart_printf("[BOOT] READY\r\n");
        }
    }
}
