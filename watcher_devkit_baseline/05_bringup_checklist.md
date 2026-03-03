# 05 Bring-up Checklist

## 目标
提供统一的首次接入检查与 10 分钟冒烟流程，快速确认开发板处于“可开发”状态。

## 适用范围
- 适用于新电脑、新环境、重装工具链、设备重插拔后的快速健康检查。
- 适用于功能开发前的硬件/固件前置确认。

## 固化事实
| 项目 | 结论/要求 | 证据 |
| --- | --- | --- |
| 串口判定必须基于连通性 | 不能只靠端口名判断，必须 `esptool chip-id` 实测 | 实测流程 |
| 高风险分区必须只读优先 | 首次接入先备份 `nvsfactory`，再进行开发写入 | `watcher_reference/use_cases/factory_firmware/docs/installation.md` |
| 起跑线版本固定 | 默认使用 ESP-IDF v5.2.1 + 官方 1.1.7 依赖线 | `04_dependency_floor_and_fallback.md` |

## 操作流程或约束
### A. 首次接入检查（5 分钟）
1. 端口枚举
```powershell
python -m serial.tools.list_ports -v
```
2. 主口判定（逐口执行，直到成功）
```powershell
python -m esptool --port COM5 chip-id
python -m esptool --port COM5 flash-id
```
3. 只读备份高风险分区（未备份则必须执行）
```powershell
python -m esptool --port COM5 --baud 2000000 --chip esp32s3 read_flash 0x9000 0x32000 nvsfactory-backup.bin
```
4. 确认当前设备事实与本目录文档一致（芯片/Flash/PSRAM/分区）。

### B. 10 分钟冒烟流程（功能前置）
1. 启动串口监控（任选可用方式）。
2. 核查日志关键点：
   - 分区表完整出现（含 `nvsfactory`、`ota_0`、`storage`）。
   - `Found 8MB PSRAM`。
   - `SPI Flash Size : 32MB`。
   - `main_task: Calling app_main()`。
3. 若需要 BSP 级验证，优先执行最小链路：
   - IO 扩展器初始化
   - SPIFFS 挂载
   - LVGL 初始化
4. 若以上通过，进入业务功能开发。

### C. 异常分流（端口/分区/依赖/启动日志）
| 项目 | 结论/要求 | 证据 |
| --- | --- | --- |
| 端口类故障 | `chip-id` 失败先查线材/接口，再换端口重试，不直接判芯片故障 | `01_device_fact_sheet.md` |
| 分区类故障 | 分区偏移不一致时先停写入，先对照 `02` 文档与官方分区表 | `02_flash_partition_recovery_baseline.md` |
| 依赖类故障 | 构建失败先回退到起跑线依赖，再做单项升级定位 | `04_dependency_floor_and_fallback.md` |
| 启动日志故障 | 无分区日志/PSRAM日志时优先排查固件与目标配置是否偏离起跑线 | `_official_fw/bootlog-70s.txt` |

## 证据来源
- `_official_fw/bootlog-70s.txt`
- `watcher_reference/use_cases/factory_firmware/docs/installation.md`
- `01_device_fact_sheet.md`
- `02_flash_partition_recovery_baseline.md`
- `04_dependency_floor_and_fallback.md`

## 变更记录
| 日期 | 变更项 | 基础版本 | 验证结果 | 风险 | 回退点 | 证据路径 |
| --- | --- | --- | --- | --- | --- | --- |
| 2026-02-27 | 初始化首次接入与 10 分钟冒烟清单 | FW 1.1.7 + ESP-IDF v5.2.1 | 已建立 | 低 | 按本清单逐项回退复核 | `watcher_devkit_baseline/` |

