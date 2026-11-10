# 验证记录

2026-11-10。以下为条件、结果与边界记录。

## 可复现检查

```powershell
python -m unittest discover -s tests -p test_upgrade.py
powershell -ExecutionPolicy Bypass -File tests/run_firmware_host.ps1
```

主机Flash模型使用clang wasm32和Node.js，编译实际CRC、Flash与描述头模块；不模拟真实缓存、Flash忙时序或断电电气过程。

## 逐字节正文编程基线

- Bootloader、小App、天气时钟构建通过；发送器5个测试方法、Flash模型32个场景通过。
- 309252字节，CRC32=85B2C329，115200 8N1，1024字节块，303块。
- 发头至DATA：2.594秒；DATA至最后ACK：32.391秒；最后ACK至DONE：0.344秒；总计35.329秒。
- 匹配所有ACK及DONE，经g启动，VTOR=0x08010000，FreeRTOS计数增长、任务数4。App全量读回与发送BIN一致。用户确认屏幕正常。
- 本地保留原始日志和固件快照，不在公开仓库复制网络配置或二进制证据。

## 先前小App异常测试

已在前一版分区/固件上验证受控发送前数据变化导致CRC拒绝，以及停止发送后接收超时回READY、无复位完整重试和启动。不能直接当作当前天气时钟大镜像的全部异常验收。

CRC错误属于按设计拒绝，不是修复了接收bug。新布局真实断电恢复尚未验证。
