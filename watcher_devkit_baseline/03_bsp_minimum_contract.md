# 03 BSP Minimum Contract

## 目标
定义板级最小初始化契约，确保项目把调试重心放在功能实现而不是硬件适配细节上。

## 适用范围
- 适用于基于 `components/sensecap-watcher` 的所有自定义应用。
- 适用于“从零 bring-up 到可进入业务逻辑”的最短路径。

## 固化事实
| 项目 | 结论/要求 | 证据 |
| --- | --- | --- |
| I/O 扩展器是电源域入口 | `bsp_io_expander_init()` 内部会设置 `BSP_PWR_SYSTEM` 并再设置 `BSP_PWR_START_UP` | `components/sensecap-watcher/sensecap-watcher.c` |
| 启动上电存在固定延时 | 关键时序含 `100ms + 50ms` 延时 | `components/sensecap-watcher/sensecap-watcher.c` |
| SD 与 SSCMA 共用 SPI2 | 初始化 SSCMA 前必须先处理 SD-CS（拉高）避免总线冲突 | `components/sensecap-watcher/sensecap-watcher.c` |
| 显示/触摸依赖 bus 就绪 | LVGL、LCD、Touch 初始化依赖 I2C/SPI 与电源域可用 | `components/sensecap-watcher/sensecap-watcher.c` |
| 音频链路依赖 I2C+I2S | 编解码器初始化依赖 bus 与 codec 设备可达 | `components/sensecap-watcher/sensecap-watcher.c` |

## 操作流程或约束
### BSP 最小初始化顺序（必须）
1. `bsp_i2c_bus_init()`
2. `bsp_io_expander_init()`（上电关键 rail）
3. `bsp_spi_bus_init()`
4. 存储二选一或并行：
   - `bsp_spiffs_init_default()`
   - `bsp_sdcard_init_default()`（若使用 SD）
5. 显示与输入：
   - `bsp_lvgl_init()`
6. 音频：
   - `bsp_codec_init()`
7. AI 链路（如使用）：
   - `bsp_sscma_client_init()`

### 关键约束（必须遵守）
1. 不允许跳过 `bsp_io_expander_init()` 直接初始化显示/音频/AI 子系统。
2. 初始化 SSCMA 时必须先拉高 `BSP_SD_SPI_CS`，避免 SD 设备与 AI 链路在 SPI2 上争用。
3. 需要低风险 bring-up 时，优先走：
   - I2C/SPI/电源域
   - SPIFFS
   - LCD/LVGL
   - 音频
   - AI 模块
4. 新项目若只做 UI 或联网，不要提前引入 SSCMA 链路，降低耦合复杂度。

### 最小可用 API 集合
| 项目 | 结论/要求 | 证据 |
| --- | --- | --- |
| 总线 | `bsp_i2c_bus_init`、`bsp_spi_bus_init`、`bsp_uart_bus_init` | `components/sensecap-watcher/include/sensecap-watcher.h` |
| 电源/扩展 IO | `bsp_io_expander_init`、`bsp_exp_io_get_level`、`bsp_exp_io_set_level` | 同上 |
| 显示 | `bsp_lvgl_init`、`bsp_lcd_brightness_set` | 同上 |
| 存储 | `bsp_spiffs_init_default`、`bsp_sdcard_init_default` | 同上 |
| 音频 | `bsp_codec_init`、`bsp_i2s_read`、`bsp_i2s_write` | 同上 |
| AI 接口 | `bsp_sscma_client_init`、`bsp_sscma_flasher_init` | 同上 |

## 证据来源
- `components/sensecap-watcher/include/sensecap-watcher.h`
- `components/sensecap-watcher/sensecap-watcher.c`
- `watcher_reference/use_cases/helloworld/main/helloworld.c`

## 变更记录
| 日期 | 变更项 | 基础版本 | 验证结果 | 风险 | 回退点 | 证据路径 |
| --- | --- | --- | --- | --- | --- | --- |
| 2026-02-27 | 初始化 BSP 最小契约 | FW 1.1.7 + ESP-IDF v5.2.1 | 已建立 | 中（初始化顺序错误会导致连锁失败） | 回到本文件定义顺序 | `components/sensecap-watcher/*` |

