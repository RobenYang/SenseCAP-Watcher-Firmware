# 04 Storage SPIFFS SD

## 目标
明确 SPIFFS 与 SD 卡初始化路径、分区约束和共享总线注意事项。

## 适用范围
- 本地文件系统挂载。
- 资源文件读取与模型/固件文件管理。

## 固化事实
| 项目 | 结论/要求 | 证据 |
| --- | --- | --- |
| SPIFFS 默认分区 | partition_label 为 storage，默认不自动格式化 | components/sensecap-watcher/sensecap-watcher.c |
| SPIFFS 默认入口 | bsp_spiffs_init_default | 同上与头文件 |
| SD 接口 | SDSPI 走 SPI2，CS 为 BSP_SD_SPI_CS(GPIO46) | 同上与头文件 |
| SD 初始化依赖 | 初始化 SD 前会先调用 bsp_spi_bus_init 与 bsp_io_expander_init | C 文件 |
| 默认挂载点 | SPIFFS: /spiffs, SD: /sdcard | include/sensecap-watcher.h |

## 操作流程或约束
1. 首次接入前先按基线文档备份高风险分区（nvsfactory）。
2. 使用 SD 与 SSCMA 时特别关注 SPI2 共享冲突。
3. SPIFFS 挂载失败先核对分区表与 partition_label 一致性。
4. 禁止在未确认分区布局前直接执行擦写高风险分区。

## 证据来源
- components/sensecap-watcher/sensecap-watcher.c
- components/sensecap-watcher/include/sensecap-watcher.h
- watcher_devkit_baseline/02_flash_partition_recovery_baseline.md
- watcher_devkit_baseline/05_bringup_checklist.md

## 变更记录
| 日期 | 变更项 | 基础版本 | 验证结果 | 风险 | 回退点 | 证据路径 |
| --- | --- | --- | --- | --- | --- | --- |
| 2026-03-02 | 初始化存储模块调试文档 | FW 1.1.7 + ESP-IDF v5.2.1 | 已建立 | 中 | 回退到官方分区与默认挂载配置 | components/sensecap-watcher/* |