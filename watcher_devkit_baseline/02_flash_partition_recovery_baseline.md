# 02 Flash Partition & Recovery Baseline

## 目标
固化分区布局、安全刷写边界和恢复资产索引，确保开发迭代不误伤高风险分区并可快速回退。

## 适用范围
- 适用于本仓库官方固件线（V1.1.7）对应的 ESP32-S3 设备。
- 适用于首次接管设备、持续迭代、异常回退三个阶段。

## 固化事实
### 分区基线（高优先级）
| 项目 | 结论/要求 | 证据 |
| --- | --- | --- |
| `nvsfactory` | `0x00009000`, `0x00032000` (200K)，高风险分区 | `_official_fw/bootlog-70s.txt`、`_official_fw/V1.1.7/V1.1.7/partition_table/partition-table.bin`、`watcher_reference/use_cases/factory_firmware/partitions.csv` |
| `nvs` | `0x0003B000`, `0x000D2000` (840K) | 同上 |
| `otadata` | `0x0010D000`, `0x00002000` (8K) | 同上 |
| `phy_init` | `0x0010F000`, `0x00001000` (4K) | 同上 |
| `ota_0` | `0x00110000`, `0x00C00000` (12M) | 同上 |
| `ota_1` | `0x00D10000`, `0x00C00000` (12M) | 同上 |
| `model` | `0x01910000`, `0x00100000` (1M) | 同上 |
| `storage` | `0x01A10000`, `0x005F0000` (6080K) | 同上 |

### 恢复资产现状
| 项目 | 结论/要求 | 证据 |
| --- | --- | --- |
| `nvsfactory` 备份 | 当前备份样本为全 `0xFF`（不可恢复出工厂凭据） | `_official_fw/nvsfactory-COM5-20260227-120519.bin` |
| 分区快照 | 已有 `pt-after.bin`、`nvsfactory-COM5-after.bin` | `_official_fw/recovery_bundle/` |
| 启动证据 | 已有 70s 启动日志用于行为对照 | `_official_fw/bootlog-70s.txt` |

## 操作流程或约束
### 只读备份命令模板（推荐）
```powershell
# 1) 列端口
python -m serial.tools.list_ports -v

# 2) 主口判定（只读）
python -m esptool --port COM5 chip-id
python -m esptool --port COM5 flash-id

# 3) 备份高风险分区（只读）
python -m esptool --port COM5 --baud 2000000 --chip esp32s3 read_flash 0x9000 0x32000 nvsfactory-backup.bin

# 4) 备份分区表（只读）
python -m esptool --port COM5 --baud 2000000 --chip esp32s3 read_flash 0x8000 0x1000 partition-table-backup.bin
```

### 安全刷写策略（默认）
1. 优先 `app-flash` 思路，仅更新应用分区，不覆盖分区表与 `nvsfactory`。
2. 刷写前必须完成只读备份，并保留时间戳命名。
3. 若需全量刷写，必须先确认恢复资产完整且回退路径可执行。

### 严禁操作清单
- `erase_flash`（整片擦除）。
- 对 `nvsfactory` 分区执行 `write_flash`。
- 未备份前改写 `partition-table.bin` 对应区域。
- 将未知来源镜像直接覆盖 `otadata`/`phy_init`。

### 恢复资产索引（当前仓库）
| 项目 | 结论/要求 | 证据 |
| --- | --- | --- |
| 固件包 | 官方固件包与解压资产 | `_official_fw/V1.1.7.zip`、`_official_fw/V1.1.7/` |
| 分区与 NVS 快照 | 备份与对比文件 | `_official_fw/nvsfactory-COM5-20260227-120519.bin`、`_official_fw/nvsfactory-COM5-after.bin`、`_official_fw/pt-after.bin` |
| 启动日志 | 上电与分区枚举证据 | `_official_fw/bootlog-70s.txt` |
| 汇总包 | 一次性恢复包 | `_official_fw/recovery_bundle/`、`_official_fw/recovery_bundle.zip` |

## 证据来源
- `_official_fw/V1.1.7/V1.1.7/partition_table/partition-table.bin`
- `_official_fw/bootlog-70s.txt`
- `_official_fw/nvsfactory-COM5-20260227-120519.bin`
- `watcher_reference/use_cases/factory_firmware/partitions.csv`
- `watcher_reference/use_cases/factory_firmware/docs/installation.md`

## 变更记录
| 日期 | 变更项 | 基础版本 | 验证结果 | 风险 | 回退点 | 证据路径 |
| --- | --- | --- | --- | --- | --- | --- |
| 2026-02-27 | 初始化分区与恢复基线 | FW 1.1.7 + ESP-IDF v5.2.1 | 已建立 | 中（误刷风险） | `recovery_bundle` 与官方固件包 | `_official_fw/*` |

