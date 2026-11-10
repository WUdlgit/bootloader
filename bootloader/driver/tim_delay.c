#include "tim_delay.h"
#include "stm32f4xx.h"

static volatile uint32_t milliseconds;

void tim_delay_init(void)
{
    SystemCoreClockUpdate();
    SysTick_Config(SystemCoreClock / 1000U);
}

void SysTick_Handler(void)
{
    milliseconds++;
}

uint32_t tim_delay_get_tick(void)
{
    return milliseconds;
}
