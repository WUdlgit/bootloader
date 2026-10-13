#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "board.h"
#include "stm32f4xx.h"

static char rxbuf[1024];
uint32_t length;
uint32_t received = 0;    // 实际收到的字节数

/* Implemented in app_handoff.s; execution continues in the App. */
void app_handoff(uint32_t stack, uint32_t entry)
    __attribute__((noreturn));

bool receive_line(char *buf, uint32_t capacity)
{
    uint32_t used = 0;

    if (buf == NULL || capacity < 2)
        return false;

    buf[0] = '\0';
    while (1)
    {
        while (USART_GetFlagStatus(USART1, USART_FLAG_RXNE) == RESET);

        char data = USART_ReceiveData(USART1);
        if (used >= capacity - 1)
            return false;

        buf[used++] = data;
        buf[used] = '\0';
        if (data == '\n')
            return true;
    }
}

bool parse_length(const char *line, uint32_t *out_length)
{
    if (line == NULL || out_length == NULL)
        return false;

    const char *start = strstr(line, "SIZE=");
    uint32_t parsed_length;
    if (start == NULL || sscanf(start, "SIZE=%lu\r\n", &parsed_length) != 1)
        return false;

    *out_length = parsed_length;
    return true;
}

bool store_in_rxbuf(char *buf, uint32_t capacity, uint32_t length)
{
    received = 0;
    if (buf == NULL || length == 0 || length > capacity)
        return false;

    for (uint32_t i = 0; i < length; i++)
    {
        while (USART_GetFlagStatus(USART1, USART_FLAG_RXNE) == RESET);

        char data = USART_ReceiveData(USART1);
        buf[received++] = data;
    }

    printf("received = %lu\r\n", (unsigned long)received);
    for (uint32_t i = 0; i < received; i++)
        printf("%02X%s", (unsigned int)(uint8_t)buf[i], i + 1 == received ? "\r\n" : " ");
    return true;
}

int main(void)
{
    board_lowlevel_init();
    board_init();
    printf("[BOOT] bare-metal base ready\r\n");

    uint32_t app_stack = *(const uint32_t *)0x08040000;
    uint32_t app_entry = *(const uint32_t *)0x08040004;

    printf("App stack: 0x%08lX\r\n", (unsigned long)app_stack);
    printf("App entry: 0x%08lX\r\n", (unsigned long)app_entry);

    while (USART_GetFlagStatus(USART1, USART_FLAG_TC) == RESET);

    while (1)
    {
        while (USART_GetFlagStatus(USART1, USART_FLAG_RXNE))
        {
            char data = USART_ReceiveData(USART1);
            if (data == 'g')
                goto app;

            if (data == 'u')
            {
                printf("已进入升级模式\r\n");
                if (receive_line(rxbuf, sizeof(rxbuf)) && parse_length(rxbuf, &length))
                {
                    printf("size = %lu\r\n", (unsigned long)length);
                    if (!store_in_rxbuf(rxbuf, sizeof(rxbuf), length))
                        printf("[BOOT] receive failed\r\n");
                }
                else
                    printf("[BOOT] invalid size\r\n");
            }
        }
    }
    
app:
    app_handoff(app_stack, app_entry);
}
