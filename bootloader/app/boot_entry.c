#include "boot_entry.h"
#include "stm32f4xx.h"
#include "tim_delay.h"

void boot_entry_init(void)
{
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC, ENABLE);
    GPIO_InitTypeDef gpio;
    GPIO_StructInit(&gpio);
    gpio.GPIO_Pin = GPIO_Pin_4;
    gpio.GPIO_Mode = GPIO_Mode_IN;
    gpio.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_Init(GPIOC, &gpio);
}

bool boot_entry_take_request(void)
{
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR, ENABLE);
    uint32_t previous = PWR->CR & PWR_CR_DBP;
    PWR->CR |= PWR_CR_DBP;
    (void)PWR->CR;
    __DSB();
    uint32_t request = RTC->BKP19R;
    RTC->BKP19R = 0;
    (void)RTC->BKP19R;
    __DSB();
    if (!previous)
        PWR->CR &= ~PWR_CR_DBP;
    return request == BOOT_REQUEST_MAGIC;
}

bool boot_entry_key_pressed(void)
{
    if (GPIO_ReadInputDataBit(GPIOC, GPIO_Pin_4) != Bit_RESET)
        return false;
    uint32_t start = tim_delay_get_tick();
    while ((uint32_t)(tim_delay_get_tick() - start) < 20U)
    {
        if (GPIO_ReadInputDataBit(GPIOC, GPIO_Pin_4) != Bit_RESET)
            return false;
    }
    return true;
}
