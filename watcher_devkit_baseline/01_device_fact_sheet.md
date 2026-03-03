# 01 Device Fact Sheet

## 目标
固化当前 Watcher 设备的最小硬件事实与串口识别结论，作为所有项目的统一起点，避免把功能调试时间消耗在端口和芯片识别上。

## 适用范围
- 适用于本仓库对应的 SenseCAP Watcher 设备。
- 适用于“把设备当开发板”进行自定义固件与应用开发的前置确认阶段。

## 固化事实
| 项目 | 结论/要求 | 证据 |
| --- | --- | --- |
| 主控芯片 | ESP32-S3（revision v0.2） | `_official_fw/bootlog-70s.txt`、`python -m esptool --port COM5 chip-id` 实测 |
| Flash 容量 | 32MB | `_official_fw/bootlog-70s.txt`、`python -m esptool --port COM5 flash-id` |
| Flash 厂商 | WinBond（日志显示） | `_official_fw/bootlog-70s.txt` |
| PSRAM | 8MB（AP_3v3，80MHz） | `_official_fw/bootlog-70s.txt`、`python -m esptool --port COM5 chip-id` |
| 安全状态 | Secure Boot 未启用 | `python -m espefuse --port COM5 summary` |
| 安全状态 | Flash Encryption 未启用 | `python -m espefuse --port COM5 summary` |
| 下载模式 | 未被 eFuse 禁用，可正常下载/刷写 | `python -m espefuse --port COM5 summary` |
| 目标平台 | `esp32s3` | `watcher_reference/use_cases/factory_firmware/sdkconfig.defaults` |

### 串口判别规则（统一执行）
| 项目 | 结论/要求 | 证据 |
| --- | --- | --- |
| 串口枚举 | 先列出系统可见串口，再逐个做 esptool 连通性判定 | `python -m serial.tools.list_ports -v` |
| 主口判定标准 | `python -m esptool --port <PORT> chip-id` 能连接并返回 ESP32-S3，即判定为 ESP 主开发口 | 实测规则 |
| 非主口判定标准 | 无法连接并报 `No serial data received` 的口，不能作为 ESP 主开发口 | `python -m esptool --port COM6 chip-id` 实测 |
| 当前结论 | 当前主开发口为 `COM5`（流程上仍以连通性判定为准） | 实测记录 |

### 设备标识（全脱敏展示）
| 项目 | 结论/要求 | 证据 |
| --- | --- | --- |
| Wi-Fi MAC | `DC:B4:D9:**:**:**` | `_official_fw/DEVELOPMENT_BASELINE_20260227.md`、`_official_fw/bootlog-70s.txt` |
| BLE MAC | `DC:B4:D9:**:**:**` | `_official_fw/bootlog-70s.txt` |
| SN | `1149************0015` | `_official_fw/DEVELOPMENT_BASELINE_20260227.md` |
| board UUID | `b488************a168` | `_official_fw/nvs-default-minimal.txt` |

## 操作流程或约束
1. 必须先判定主口，再做任何读写命令。
2. 端口号（例如 `COM5`）是“当前样本结论”，不是硬编码约束；跨机器或重插拔后必须重判。
3. 若 `chip-id` 与 `flash-id` 任一失败，先排查线材/接口，再排查端口归属，不要直接判断为硬件损坏。

## 证据来源
- `README.md`
- `_official_fw/DEVELOPMENT_BASELINE_20260227.md`
- `_official_fw/bootlog-70s.txt`
- `_official_fw/nvs-default-minimal.txt`
- `watcher_reference/use_cases/factory_firmware/sdkconfig.defaults`

## 变更记录
| 日期 | 变更项 | 基础版本 | 验证结果 | 风险 | 回退点 | 证据路径 |
| --- | --- | --- | --- | --- | --- | --- |
| 2026-02-27 | 初始化设备事实单 | FW 1.1.7 + ESP-IDF v5.2.1 | 已建立 | 低 | 使用 `_official_fw` 既有快照 | `_official_fw/*` |

