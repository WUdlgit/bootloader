# STM32F407 单 App 串口 IAP

STM32F407VET6 裸机 Bootloader：串口分块接收、内部 Flash 擦写与读回、CRC32、描述头提交、校验后启动独立 App。

## 配置

| 区域 | 地址 | 容量 |
| --- | --- | --- |
| Bootloader（扇区0～2） | 0x08000000～0x0800C000 | 48 KiB |
| 描述头（扇区3） | 0x0800C000～0x08010000 | 16 KiB |
| App（扇区4～7） | 0x08010000～0x08080000 | 448 KiB |

USART3 升级：PD8 TX、PD9 RX，115200 8N1。USB转TTL TXD接PD9、RXD接PD8、共地，使用3.3V逻辑电平，模块VCC不接。USART1 PA9/PA10用于日志。PB10/PB11留给天气时钟I2C2。

## 构建与使用

工具：Arm GNU Toolchain、GNU Make；板端使用ST-Link/OpenOCD。Python发送器依赖pyserial。

```powershell
make -C bootloader -j4
make -C test_app -j4
python -m pip install pyserial
python tools/upgrade.py test_app/build/test_app.bin --check-only
python tools/upgrade.py test_app/build/test_app.bin --port COM9 --boot
```

COM9只是示例，应选择实际USART3适配器端口。先通过SWD烧录bootloader/build/bootloader.elf，然后复位运行。升级App必须链接到0x08010000，SystemInit中的向量偏移也必须为0x10000。不能直接发送链接在0x08000000的独立工程BIN。

天气时钟接入说明见 [docs/weather-clock-integration.md](docs/weather-clock-integration.md)。测试命令与板端验证见 [docs/validation.md](docs/validation.md)。

## 协议与完整性

电脑发`?`等READY，发`u`等HEADER，提交`SIZE=十进制长度 CRC=八位十六进制CRC\n`。设备先擦除旧描述头，再擦所需App扇区，回复DATA。正文按1024字节分块，逐块写入和读回后发送累计字节ACK。整包向量/CRC检查通过后保存描述头，magic最后写，随后DONE。发送`g`才校验并交接启动；上电不自动运行App。

接收采用字节间3秒超时；失败清理残留输入，连续空闲100ms后回READY。重试从头开始。CRC32与Python zlib一致，只提供损坏检测，不提供身份认证。

## 范围

单App原地更新：擦写过程中可能破坏旧App，失败后通过Bootloader重新下载。没有回滚、断点续传、外部Flash暂存、签名或OTA。固件测试成功不代表全部业务和真实断电恢复都验证完成。

源码位于bootloader/，独立小App位于test_app/，电脑发送器位于tools/。原始日志、整片Flash备份、固件快照和个人资料留在本地outputs/，不发布。
