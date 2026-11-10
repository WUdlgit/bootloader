#include "console.h"
#include "stm32f4xx.h"

/* 供调试器观察，只在 App 的 TIM2 中断中递增 */
volatile uint32_t app_timer_interrupt_count = 0;

static void led_init(void)
{
    GPIO_InitTypeDef gpio;

    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);
    GPIO_SetBits(GPIOB, GPIO_Pin_0); /* LED1 低电平点亮，先设置为熄灭 */
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
    /* APB1 分频不为 1 时，定时器时钟是 APB1 外设时钟的两倍 */
    if ((RCC->CFGR & RCC_CFGR_PPRE1) != 0)
        timer_clock *= 2;

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);
    TIM_DeInit(TIM2);
    TIM_TimeBaseStructInit(&timer);
    timer.TIM_Prescaler = (uint16_t)(timer_clock / 10000U - 1U);
    timer.TIM_Period = 10000U - 1U; /* 计数频率 10 kHz，每秒产生一次更新 */
    timer.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM2, &timer);
    /* 初始化会产生一次更新事件，开启中断前先清除它 */
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
    /* 启动文件已在进入 main 前调用 SystemInit */
    SystemCoreClockUpdate();
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE);
    console_init();

    console_write("Hello world | App v0.2\r\n");
    led_init();
    timer_init();

    while (1)
    {
        __NOP();
    }
}
