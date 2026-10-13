# 第三方与移植代码说明

本仓库包含原创实现、移植的基础工程以及第三方 MCU 支持代码。
根目录 MIT 许可证只覆盖本项目原创部分，不覆盖或替换第三方许可条款。

- `bootloader/firmware/` 和 `test_app/firmware/`：STMicroelectronics 标准外设库、设备支持文件及 ARM CMSIS。请保留并遵守各文件原有版权和许可声明。
- 一些 ST 文件引用独立的 `LICENSE` 文件；当前工作副本没有附带该上游文件。根目录的 MIT 许可证不是该上游许可证，不应据此推定这些文件按 MIT 授权。
- `bootloader/driver/`、`test_app/driver/` 以及复制或调整的基础构建、板级支持文件：来自既有 Weather_Clock 学习工程工作副本，保留其来源与原有声明。

项目来源与 AI 辅助范围见 README 及验证记录。发布到公开仓库不改变第三方代码的权利归属。
