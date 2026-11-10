#ifndef __BL_USART_H__
#define __BL_USART_H__

#include <stdbool.h>
#include <stdint.h>

/* 升级专用串口：USART3，PB10 发送、PB11 接收 */
void bl_usart_init(void);
bool bl_usart_read_byte(uint8_t *data, uint32_t timeout);
void bl_usart_printf(const char *format, ...)
    __attribute__((format(printf, 1, 2)));

#endif
