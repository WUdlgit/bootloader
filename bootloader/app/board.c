#include "board.h"
#include "console.h"
#include "bl_usart.h"
#include "led_desc.h"
#include "led.h"
#include "stm32f4xx.h"

static struct led_desc _led1 = {GPIOB, GPIO_Pin_0, Bit_RESET, Bit_SET};
static struct led_desc _led2 = {GPIOB, GPIO_Pin_1, Bit_RESET, Bit_SET};
static struct led_desc _led3 = {GPIOE, GPIO_Pin_9, Bit_RESET, Bit_SET};
static led_desc_t led1 = &_led1;
static led_desc_t led2 = &_led2;
static led_desc_t led3 = &_led3;

void board_lowlevel_init(void)
{
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOD, ENABLE);
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOE, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART3, ENABLE);
}

void board_init(void)
{
    led_init(led1);
    led_init(led2);
    led_init(led3);
    console_init();
    bl_usart_init();
}

/* GCC(newlib-nano) 的 printf 走 _write，而不是 fputc；不实现它串口就没输出 */
int _write(int file, char *ptr, int len)
{
    for (int i = 0; i < len; i++)
    {
        while (!USART_GetFlagStatus(USART1, USART_FLAG_TXE))
            ;
        USART_SendData(USART1, (uint8_t)ptr[i]);
    }
    return len;
}
