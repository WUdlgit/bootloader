# Hello world 测试 App

这是用于验证 Bootloader 到独立 App 交接的最小应用工程。
基础启动代码、芯片库和串口驱动复制自相邻的 `bootloader` 工程。

## 行为

启动代码调用 `SystemInit()`，准备 RAM 数据，然后进入 `app/main.c`。
App 自行开启 GPIOA、USART1 时钟并初始化串口，阻塞发送一次
`Hello world | App v0.1\r\n`，然后初始化 LED1 和 TIM2，主循环保持空转。
TIM2 每秒触发一次更新中断，在 `TIM2_IRQHandler()` 中翻转 LED1（PB0，
低电平点亮），所以亮一秒、灭一秒，完整亮灭周期为两秒。
`app_timer_interrupt_count` 只在这个中断里增加，供调试器检查。

- USART1：PA9 为 TX，PA10 为 RX。
- 串口参数：115200、8 数据位、无校验、1 停止位、无流控。
- 沿用基础工程的 HSE 8 MHz 配置；TIM2 按当前 APB1 时钟计算分频。
- 不启用串口接收中断、SysTick 或 RTOS。

## 地址与产物

- 链接 Flash 区域：`0x08040000`，长度 256 KB，使用扇区 6～7。
- App 向量表：`0x08040000`。
- `SystemInit()` 的 `VECT_TAB_OFFSET`：`0x40000`。
- RAM：沿用基础工程的 `0x20000000`、128 KB。
- 在本目录执行 `make -j8`，生成 `build/test_app.elf`、`.hex`、`.bin` 和 `.map`。
- HEX 含烧录地址；BIN 不含地址，应写入 `0x08040000`。

这份 App 放在偏移地址，单纯烧录后复位不会自动从该地址启动。
它需要 Bootloader 正确交接，或由调试器建立正确的启动状态。
相邻 Bootloader 工程按前 256 KB Flash 链接。已有交接验证证明复位后
能够进入 App 并输出 `Hello world`；TIM2 中断属于随后增加的验证内容。
编译、烧录、调试器观测和人眼看到 LED 闪烁应分别记录。

当前开发基线与阶段验收见 [开发基线](../开发基线.md)。版本文字的最新板上
验证由学员在对话中确认，与之前的调试器和串口日志分别记录。
