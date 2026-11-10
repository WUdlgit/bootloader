# Bootloader

入口app/main.c完成初始化，app/bootloader.c负责协议和交接，app/magic_header.c负责完整性及描述头，driver/stm32_flash.c负责范围受限的擦写和读回，app/jumpapp.s负责切栈跳转。

App固定地址0x08010000，最大448 KiB；USART3 PD8/PD9，115200 8N1。完整操作见[根README](../README.md)。
