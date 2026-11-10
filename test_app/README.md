# 独立测试 App

使用GNU Make构建，链接地址0x08010000，VTOR偏移0x10000。USART1打印App v0.2，TIM2中断增加计数并翻转PB0，用于检查Bootloader交接。它不是天气时钟，也不能替代天气时钟业务验证。

构建与串口发送见[根README](../README.md)。
