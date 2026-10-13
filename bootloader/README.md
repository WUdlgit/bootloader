# Bootloader 裸机基础工程

来源：`../STM32F4_FreeRTOS/WT_FreeRTOS_ESPAT_UI` 当前工作副本。
本目录是独立的 GCC/Make 工程；原天气时钟未修改。

## 当前保留

- CMSIS、原 GCC 启动文件、SystemInit、原 GNU 链接脚本。
- 标准外设库头文件及 GPIO/RCC/USART/FLASH/TIM/EXTI/SYSCFG/NVIC 源文件。
- USART1 日志基础：PA9/PA10，115200；沿用原 GPIO/USART 配置。
- LED 和按键驱动、printf 的 GCC `_write` 重定向。
- Makefile、格式化配置、VS Code 构建和 ST-Link 调试配置。

## 本次精简

未带入天气、WiFi、ESP-AT、LCD、UI、字库图片、AHT20、RTC、EEPROM、FreeRTOS、历史构建产物、缓存、Keil 工程和本地凭据。
串口移除了 FreeRTOS 信号量、DMA 和 RX 中断/回调；当前使用轮询收发。
`main()` 初始化基础外设，读取 App 的栈地址和启动入口，输出后等待串口字符。
收到 `u` 打印升级模式提示并继续等待，收到 `g` 通过 `app_handoff()` 进入 App。
按键驱动保留原实现，但基础 main 尚未启用按键；原 `key_init()` 会开启 EXTI，使用前需要配套处理相应 IRQ 或自行调整按键方案。
LED 编号/引脚映射沿用当前原工程，未根据外部说明重新编号。

## 构建与当前边界

在本目录执行 `make -j8`，得到 `build/bootloader.elf`、`.hex`、`.bin`。
`make compiledb` 沿用原工程的 Python compiledb 工具，用于刷新 clangd 数据库。

**当前已验证 App 跳转和串口选择入口，尚不具备固件升级功能。**
固件接收协议、Flash 擦写、固件格式/校验及恢复逻辑仍待实现。
Bootloader 链接 Flash 起址为 `0x08000000`，容量限制到前 256 KiB；
独立 App 从 `0x08040000` 开始。元数据存放方式尚未确定。
当前板上验证、范围与分区见 [开发基线](../开发基线.md)。

构建记录（2026-10-08）：GCC 编译、链接及 ELF/HEX/BIN 生成通过。保留原 SystemInit 的缩进告警和 newlib/nosys 未实现文件系统调用的链接告警，未进行板上验证。
