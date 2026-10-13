# bootloader

STM32F407VET6 裸机 Bootloader 学习工程，包含独立构建的 Bootloader 和测试 App。

## 当前状态

已经完成启动交接、串口升级入口以及原始二进制数据接收到 RAM 的验证。
当前尚未完成固件写入 Flash、完整性校验及 App V1 → V2 的完整串口升级。

现有串口入口：

- `g`：启动位于 `0x08040000` 的测试 App。
- `u`：进入接收流程，随后接收 `SIZE=<字节数>` 和 LF 换行，再按长度接收原始字节。
- 二进制接收缓冲区容量为 1024 字节，收齐后输出数量和十六进制内容。

发送端先等升级提示，再发送长度，等 `size` 确认后再发送正文。
正文中的 `0x00` 和 `0x0A` 都属于普通数据，不作为字符串或行的结束标记。

## 目录

| 路径 | 内容 |
|---|---|
| `bootloader/` | Bootloader、驱动、芯片库及 GCC/Make 构建文件 |
| `test_app/` | 独立测试 App，串口输出版本并通过 TIM2 中断翻转 LED |
| `开发基线.md` | 分区、启动交接和早期验收记录；该文件记录日期为 2026-10-11 |
| `binary-rx-verification-20261013.md` | 2026-10-13 二进制接收验证及已知边界 |
| `verify-binary-rx.ps1` | 串口二进制接收验证脚本 |
| `verify-mode-select.ps1` | 早期 u/g 入口验证脚本，保留用于历史参考 |

Flash 备份、构建产物、缓存及原始本机日志保留在本地，不纳入版本控制。
验证记录中的历史日志链接对应本地证据文件。

## 硬件与分区

- MCU：STM32F407VET6，Flash 512 KiB。
- USART1：PA9 TX、PA10 RX，115200、8N1、无流控。
- LED1：PB0，低电平点亮。
- Bootloader：`0x08000000` 至 `0x08040000`，扇区 0～5。
- App：`0x08040000` 至 `0x08080000`，扇区 6～7。

以上结束地址均不包含在对应区域内。测试 App 的向量表和链接起点为 `0x08040000`。

## 构建

需要 Arm GNU Toolchain 和 GNU Make：

```powershell
cd bootloader
make -j8
cd ../test_app
make -j8
```

分别生成各工程 `build/` 下的 ELF、HEX、BIN 和 MAP。
`.bin` 不包含目标地址；当前测试 App 的 BIN 应写到 `0x08040000`。

验证脚本依赖 Windows PowerShell、ST-Link 和 OpenOCD。
运行前检查脚本内的工具路径及串口设置，并查阅验证记录。
`verify-binary-rx.ps1 -PortName <COM编号> -NoFlash` 只复位板子并执行串口验证。

## 来源与贡献

基础工程、部分板级驱动及构建支持由 Weather_Clock 工作副本精简而来。
芯片驱动来自 STMicroelectronics，CMSIS 组件来自 ARM；相关文件保留原有版权声明。
本项目包含学员实现以及明确授权的 AI 辅助代码、脚手架和验证操作；板上运行结果不代表所有代码均由学员独立完成。

原创部分按 [MIT 许可证](LICENSE) 发布。第三方及移植代码不因根目录许可证而重新授权，详见 [第三方说明](THIRD_PARTY_NOTICES.md)。
