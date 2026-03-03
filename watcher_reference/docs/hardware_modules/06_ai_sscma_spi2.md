# 06 AI SSCMA SPI2

## 目标
固化 SSCMA 客户端与 flasher 链路的初始化约束，避免 SPI2 争用导致 AI 不可用。

## 适用范围
- SSCMA 推理链路。
- SSCMA 固件或模型升级链路。

## 固化事实
| 项目 | 结论/要求 | 证据 |
| --- | --- | --- |
| 初始化依赖 | bsp_sscma_client_init 依赖 io expander 与 SPI bus 已就绪 | components/sensecap-watcher/sensecap-watcher.c |
| 总线冲突规避 | SSCMA 初始化前会将 BSP_SD_SPI_CS 拉高 | 同上 |
| 复位/同步引脚 | RST 与 SYNC 使用扩展 IO 配置 | C 文件与头文件 |
| SPI 参数 | SSCMA 运行在 SPI2，默认时钟 12MHz | include/sensecap-watcher.h |
| flasher 链路 | bsp_sscma_flasher_init 使用 we2 flasher 配置 | sensecap-watcher.c |

## 操作流程或约束
1. 初始化 SSCMA 前必须确认 SD 不占用 SPI2（CS 置高）。
2. 发生 AI 通信失败时优先排查 SPI2 时序、扩展 IO 复位脚状态。
3. 若仅开发 UI/联网功能，可暂不接入 SSCMA 降低耦合复杂度。
4. OTA/刷写流程中保持 reset_use_expander 配置一致。

## 证据来源
- components/sensecap-watcher/sensecap-watcher.c
- components/sensecap-watcher/include/sensecap-watcher.h
- watcher_devkit_baseline/03_bsp_minimum_contract.md

## 变更记录
| 日期 | 变更项 | 基础版本 | 验证结果 | 风险 | 回退点 | 证据路径 |
| --- | --- | --- | --- | --- | --- | --- |
| 2026-03-02 | 初始化 SSCMA 模块调试文档 | FW 1.1.7 + ESP-IDF v5.2.1 | 已建立 | 高 | 回退官方 SSCMA 初始化路径 | components/sensecap-watcher/sensecap-watcher.c |