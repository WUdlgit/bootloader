#include <stdarg.h>
#include <stdio.h>
#include "stm32f4xx.h"
#include "bl_usart.h"
#include "tim_delay.h"

/* 与老师一致：USART3 使用 PD8 发送、PD9 接收，115200、8N1 */
void bl_usart_init(void)
{
    GPIO_InitTypeDef gpio;
    USART_InitTypeDef usart;

    GPIO_PinAFConfig(GPIOD, GPIO_PinSource8, GPIO_AF_USART3);
    GPIO_PinAFConfig(GPIOD, GPIO_PinSource9, GPIO_AF_USART3);
    GPIO_StructInit(&gpio);
    gpio.GPIO_Pin = GPIO_Pin_8 | GPIO_Pin_9;
    gpio.GPIO_Mode = GPIO_Mode_AF;
    gpio.GPIO_OType = GPIO_OType_PP;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    gpio.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_Init(GPIOD, &gpio);

    USART_StructInit(&usart);
    usart.USART_BaudRate = 115200u;
    usart.USART_WordLength = USART_WordLength_8b;
    usart.USART_StopBits = USART_StopBits_1;
    usart.USART_Parity = USART_Parity_No;
    usart.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    usart.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
    USART_Init(USART3, &usart);
    USART_Cmd(USART3, ENABLE);
}

void bl_usart_printf(const char *format, ...)
{
    /* 这里只发送简短应答；固件正文直接按字节传输，不经过格式化输出 */
    char message[128];
    va_list args;
    va_start(args, format);
    int length = vsnprintf(message, sizeof(message), format, args);
    va_end(args);
    if (length <= 0)
        return;
    if ((unsigned int)length >= sizeof(message))
        length = sizeof(message) - 1;

    for (int i = 0; i < length; i++)
    {
        while (USART_GetFlagStatus(USART3, USART_FLAG_TXE) == RESET);
        USART_SendData(USART3, (uint8_t)message[i]);
    }
    while (USART_GetFlagStatus(USART3, USART_FLAG_TC) == RESET);
}

/* 接收一个字节；超时或串口报错时返回失败 */
bool bl_usart_read_byte(uint8_t *data, uint32_t timeout)
{
    uint32_t start = tim_delay_get_tick();

    while ((uint32_t)(tim_delay_get_tick() - start) < timeout)
    {
        uint32_t status = USART3->SR;
        if (status & (USART_SR_ORE | USART_SR_NE | USART_SR_FE | USART_SR_PE))
        {
            (void)USART3->DR; // 先读 SR 再读 DR，清除接收错误
            return false;
        }
        if (status & USART_SR_RXNE)
        {
            *data = (uint8_t)USART3->DR;
            return true;
        }
    }
    return false;
}
