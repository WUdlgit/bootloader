#include "console.h"
#include "stm32f4xx.h"

/* Visible in the debugger; incremented only by the App's TIM2 interrupt. */
volatile uint32_t app_timer_interrupt_count = 0;

static void led_init(void)
{
    GPIO_InitTypeDef gpio;

    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);
    GPIO_SetBits(GPIOB, GPIO_Pin_0); /* LED1 is active low: start off. */
    GPIO_StructInit(&gpio);
    gpio.GPIO_Pin = GPIO_Pin_0;
    gpio.GPIO_Mode = GPIO_Mode_OUT;
    gpio.GPIO_OType = GPIO_OType_PP;
    gpio.GPIO_Speed = GPIO_Speed_2MHz;
    gpio.GPIO_PuPd = GPIO_PuPd_NOPULL;
    GPIO_Init(GPIOB, &gpio);
}

static void timer_init(void)
{
    RCC_ClocksTypeDef clocks;
    TIM_TimeBaseInitTypeDef timer;
    NVIC_InitTypeDef interrupt;

    RCC_GetClocksFreq(&clocks);
    uint32_t timer_clock = clocks.PCLK1_Frequency;
    /* On STM32F407, an APB1 divider other than 1 doubles the timer clock. */
    if ((RCC->CFGR & RCC_CFGR_PPRE1) != 0)
        timer_clock *= 2;

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);
    TIM_DeInit(TIM2);
    TIM_TimeBaseStructInit(&timer);
    timer.TIM_Prescaler = (uint16_t)(timer_clock / 10000U - 1U);
    timer.TIM_Period = 10000U - 1U; /* 10 kHz counter: update once per second. */
    timer.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM2, &timer);
    /* TimeBaseInit generates an update; discard it before enabling IRQs. */
    TIM_ClearITPendingBit(TIM2, TIM_IT_Update);
    NVIC_ClearPendingIRQ(TIM2_IRQn);

    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4);
    interrupt.NVIC_IRQChannel = TIM2_IRQn;
    interrupt.NVIC_IRQChannelPreemptionPriority = 5;
    interrupt.NVIC_IRQChannelSubPriority = 0;
    interrupt.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&interrupt);
    TIM_ITConfig(TIM2, TIM_IT_Update, ENABLE);
    TIM_Cmd(TIM2, ENABLE);
}

void TIM2_IRQHandler(void)
{
    if (TIM_GetITStatus(TIM2, TIM_IT_Update) != RESET)
    {
        TIM_ClearITPendingBit(TIM2, TIM_IT_Update);
        GPIO_ToggleBits(GPIOB, GPIO_Pin_0);
        app_timer_interrupt_count++;
    }
}

int main(void)
{
    /* SystemInit runs in the startup file before main. */
    SystemCoreClockUpdate();
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE);
    console_init();

    console_write("Hello world | App v0.1\r\n");
    led_init();
    timer_init();

    while (1)
    {
        __NOP();
    }
}
