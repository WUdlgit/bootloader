#ifndef __TIM_DELAY_H__
#define __TIM_DELAY_H__

#include <stdint.h>

/* 使用 SysTick 提供毫秒时基，供串口接收判断超时 */
void tim_delay_init(void);
uint32_t tim_delay_get_tick(void);

#endif
