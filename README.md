# STM32F407 单 App 串口 IAP

STM32F407VET6 裸机 Bootloader：串口分块接收、内部 Flash 擦写与读回、CRC32、描述头提交、校验后启动独立 App。支持约 1 秒维护窗口、板载按键和 RTC 备份寄存器一次性请求进入升级模式。

> 公开仓库不包含原始日志、整片 Flash 备份和本地网络配置。

## 配置

| 区域 | 地址 | 容量 | 说明 |
| --- | --- | --- | --- |
| Bootloader（扇区 0～2） | 0x08000000～0x0800C000 | 48 KiB | Bootloader 代码 |
| 描述头（扇区 3） | 0x0800C000～0x08010000 | 16 KiB | magic、length、crc、info_crc |
| App（扇区 4～7） | 0x08010000～0x08080000 | 448 KiB | 单 App 原地更新 |

- 升级串口：USART3，**PD8 TX、PD9 RX**，115200 8N1；USB 转 TTL 的 TXD 接 PD9、RXD 接 PD8、共地，使用 3.3 V 逻辑电平，模块 VCC 不接。
- 日志串口：USART1，PA9 TX、PA10 RX。
- PB10/PB11 留给天气时钟的 I2C2，不用于升级。
- App 必须链接到 `0x08010000`，`SystemInit` 的 `VECT_TAB_OFFSET` 必须为 `0x10000`；不能直接发送链接在 `0x08000000` 的独立工程 BIN。

## 构建与使用

工具：Arm GNU Toolchain、GNU Make；板端使用 ST-Link/OpenOCD。Python 发送器依赖 pyserial。

```powershell
make -C bootloader -j4
make -C test_app -j4
python -m pip install pyserial
python tools/upgrade.py test_app/build/test_app.bin --check-only
python tools/upgrade.py test_app/build/test_app.bin --port COM9 --boot
```

`COM9` 只是示例，应选择实际 USART3 适配器端口。先通过 SWD 烧录 `bootloader/build/bootloader.elf`，再复位运行。

## 启动与升级入口

- **有效 App 正常上电**：约 1 秒维护窗口后校验并自动启动 App。
- **上电/复位维护**：先按住 KEY2（PC4，外部上拉、按下接地，低电平有效），再上电或按复位；识别后留在升级模式，松开按键即可操作。
- **运行中维护**：天气时钟长按 KEY2 约 2 秒，写入一次性请求到 RTC 备份寄存器 `BKP19R`（魔数 `0x49504155`），随后软件复位；Bootloader 读取并清零请求，留在升级模式。
- **窗口内命令**：窗口内收到 `?`、`u` 或 `g` 会被保留，进入命令循环后处理，不会丢失。
- **退出维护**：有效 App 未被破坏时发送 `g` 可校验并启动；App 无效时不自动跳转，需重新完整升级。
- **升级模式指示**：PB0 点亮（低电平）。
- 维护窗口约 1 秒，不包含完整 App 校验时间；长按检测受任务调度影响，不是硬实时保证。详见 [升级入口](docs/upgrade-entry.md)。

## 协议与完整性

电脑发 `?` 等 `[BOOT] READY`；发 `u` 等 `[BOOT] HEADER`；提交 `SIZE=十进制长度 CRC=八位十六进制CRC\n`。设备先擦除旧描述头，再按长度擦所需 App 扇区，回复 `[BOOT] DATA <长度> <块大小>`。正文按 1024 字节分块，逐块写入、读回后发送累计字节 ACK。整包向量/CRC 检查通过后保存描述头，magic 最后写，随后 `[BOOT] DONE`。

完整性错误会返回具体原因：

| 输出 | 含义 |
| --- | --- |
| `ERROR header` | 升级头接收或解析失败，尚未擦除 |
| `ERROR erase` | 描述头或 App 扇区擦除失败 |
| `ERROR receive N` | 正文接收失败，N 为累计接收字节数 |
| `ERROR write N` | 块写入或读回失败，N 为此前成功写入量 |
| `ERROR length` | 提交函数收到越界长度 |
| `ERROR vector` | 初始栈或复位入口非法 |
| `ERROR image_crc expected=… actual=…` | 整包 CRC 不符 |
| `ERROR header_write field=…` | 描述头字段 length/crc/info_crc/magic 写入或读回失败 |
| `ERROR header_verify` | 描述头自身检查失败 |

CRC32 与 Python `zlib.crc32` 一致，只提供损坏检测，不提供身份认证。接收采用字节间 3 秒超时；失败后清理残留输入，连续空闲 100 ms 后回 READY，重试从头开始。

## 测试

```powershell
python -m unittest discover -s tests -p test_upgrade.py
powershell -ExecutionPolicy Bypass -File tests/run_firmware_host.ps1
```

- 发送器 5 个单元测试：多块/尾块/扇区边界、失败停止、无效镜像、应答超时、短写。
- 75 个主机模型场景：实际 CRC/描述头/Flash 模块替换底层 Flash API，覆盖边界、损坏、错误注入、提交中断、32 位编程对齐和详细错误返回。
- 主机模型不模拟真实缓存、Flash 忙时序、断电电气过程，不能替代板端验证。

最近板端记录（详见 [验证记录](docs/validation.md)）：

- 309252 字节天气时钟经 USART3 完成 303 块升级，全部 ACK/DONE，全片读回与 BIN 一致；`g` 启动后 VTOR=0x08010000，FreeRTOS 运行。
- 同一镜像、同一发送器条件下，正文改为对齐 32 位字＋首尾字节编程后，单次对照总时长由 35.329 s 降至 31.797 s（约 10%）；这是单次对照，不是稳定吞吐承诺。
- 已用板端验证无效向量、错误 CRC、未提交描述头的具体错误返回，以及 KEY2 按键复位进入升级模式。

## 范围与限制

- 单 App 原地更新：擦写过程中可能破坏旧 App，失败后通过 Bootloader 重新完整下载。
- 没有回滚、断点续传、外部 Flash 暂存、签名或云端 OTA。
- 新布局的真实断电恢复、长时间稳定运行和全部业务交互尚未验证。
- 新增入口、细分错误和 32 位编程只在 GCC/Make 工程中接入；Keil 工程未同步新增源文件，不应混用生成物。

源码位于 `bootloader/`，独立测试 App 位于 `test_app/`，电脑发送器位于 `tools/`。原始日志、整片 Flash 备份和固件快照留在本地 `outputs/`，不发布。
