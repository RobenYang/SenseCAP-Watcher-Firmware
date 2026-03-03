# 04 Dependency Floor & Fallback

## 目标
固化“可复现起跑线”与“已验证依赖保底线”，保证重开发时先可运行、再增量优化。

## 适用范围
- 适用于本仓库所有基于 ESP-IDF 的 Watcher 开发任务。
- 适用于新项目立项、依赖裁剪、版本升级前评估。

## 固化事实
### 可复现起跑线（必须先满足）
| 项目 | 结论/要求 | 证据 |
| --- | --- | --- |
| 官方固件版本 | `1.1.7` | `_official_fw/V1.1.7/V1.1.7/factory_firmware.bin`（image-info） |
| 官方构建 IDF | `v5.2.1` | `_official_fw/V1.1.7/V1.1.7/factory_firmware.bin`（image-info） |
| 目标芯片 | `esp32s3` | `watcher_reference/use_cases/factory_firmware/sdkconfig.defaults` |
| Flash 能力项 | `CONFIG_ESPTOOLPY_FLASHSIZE_32MB=y` | `watcher_reference/use_cases/factory_firmware/sdkconfig.defaults` |
| PSRAM 能力项 | `CONFIG_SPIRAM_MODE_OCT=y`、`CONFIG_SPIRAM_SPEED_80M=y` | 同上 |
| 蓝牙能力项 | `CONFIG_BT_NIMBLE_ENABLED=y` | 同上 |

### 已验证依赖（官方工程可用）
| 项目 | 结论/要求 | 证据 |
| --- | --- | --- |
| BSP 组件 | `sensecap-watcher`（本地 override） | `watcher_reference/use_cases/factory_firmware/main/idf_component.yml` |
| IO 扩展器 | `esp_io_expander_pca95xx_16bit`（本地 override） | 同上 |
| LCD 驱动 | `espressif/esp_lcd_spd2010:1.0.1` | `components/sensecap-watcher/idf_component.yml` |
| Touch 驱动 | `espressif/esp_lcd_touch_spd2010:0.0.1` | 同上 |
| 音频设备抽象 | `espressif/esp_codec_dev:1.2.0` | 同上 |
| LED Strip | `led_strip:2.5.4` | 同上 |
| Button | `button:3.2.3` | 同上 |
| Knob | `knob:0.1.4` | 同上 |
| 语音包 | `espressif/esp-sr:1.7.1` | `watcher_reference/use_cases/factory_firmware/main/idf_component.yml` |
| 音频播放器 | `chmorgan/esp-audio-player:1.0.6` | 同上 |

## 操作流程或约束
### 依赖分层（保底策略）
1. 核心必须层（开发板可用性）：
   - `sensecap-watcher`
   - `esp_io_expander_pca95xx_16bit`
   - `esp_lcd_spd2010`
   - `esp_lcd_touch_spd2010`
   - `esp_codec_dev`
2. 功能增强层（按项目引入）：
   - `esp-sr`
   - `esp-audio-player`
   - `esp-file-iterator`
   - 其他业务组件

### 版本策略（默认执行）
1. 先锁定起跑线：`ESP-IDF v5.2.1 + 官方 1.1.7 对应依赖`。
2. 允许后续升级，但每次升级都必须：
   - 在 `06_compatibility_log.md` 写明验证结果。
   - 标注风险、回退点、证据路径。
3. 若升级失败，立即回到起跑线版本继续开发，避免阻塞主线功能。

## 证据来源
- `README.md`
- `_official_fw/V1.1.7/V1.1.7/factory_firmware.bin`
- `watcher_reference/use_cases/factory_firmware/sdkconfig.defaults`
- `watcher_reference/use_cases/factory_firmware/main/idf_component.yml`
- `components/sensecap-watcher/idf_component.yml`

## 变更记录
| 日期 | 变更项 | 基础版本 | 验证结果 | 风险 | 回退点 | 证据路径 |
| --- | --- | --- | --- | --- | --- | --- |
| 2026-02-27 | 初始化依赖起跑线与保底分层 | FW 1.1.7 + ESP-IDF v5.2.1 | 已建立 | 中（升级不当会破坏可复现性） | 回退到起跑线版本 | `watcher_reference/use_cases/factory_firmware/main/idf_component.yml` |

