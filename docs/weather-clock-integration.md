# 天气时钟接入

目标工程：[weather_clock](https://github.com/WUdlgit/weather_clock)，目录WT_FreeRTOS_ESPAT_UI，使用GCC/Make构建。本轮不调整Keil链接配置。

1. 链接脚本FLASH ORIGIN设为0x08010000、LENGTH设为448K。
2. SystemInit使用VECT_TAB_OFFSET=0x10000。
3. 全量重新构建，使用build/stm32f407.bin，经本仓库tools/upgrade.py发送。
4. 天气时钟PB10/PB11用于传感器I2C2，升级使用PD8/PD9，避免信号冲突。

已实测309252字节天气时钟经串口下载、DONE、g启动；读回BIN一致，向量表地址正确，FreeRTOS时基增长，有网络时间和天气响应。用户报告LCD显示正常。没有验证全部交互、长期运行或真实断电恢复。

不要发布含本地网络或服务配置的BIN、原始控制台日志和整片Flash备份。
