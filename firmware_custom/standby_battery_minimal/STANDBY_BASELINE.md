# 待机续航基底说明

## 目标
- 明确本固件为何是“待机续航基底”。
- 固化当前版本的低功耗设计策略，避免后续加功能时把待机功耗悄悄拉高。
- 给后续开发者一份可直接执行的“扩展前检查单”。

## 基底定位
- 本固件不是通用应用框架，而是 **SenseCAP Watcher 自研低功耗基底**。
- 本基底优先保证三件事：
  1. 待机路径短且稳定。
  2. 未使用硬件默认不上电。
  3. 电量采样尽量在低负载窗口进行，降低显示负载对估算结果的污染。
- 后续如果要在此基础上增加功能，必须先确认：
  1. 新版本的待机功耗与本基底版本对齐；
  2. 新增功能只在需要时才上电；
  3. 每新增一项能力，都要重新验证深睡前 rail 状态、唤醒路径与电量估算稳定性。

## 当前结论
- 当前实测续航仍可接受。
- 因此，本分支应作为后续“在低功耗前提下逐步加功能”的起点，而不是再沿用原先与功能不匹配的分支命名。

## 续航优化策略

### 1. 电源域最小化
- 除显示、系统基础电源与必要采样链路外，默认不打开额外硬件。
- 当前常态关闭的 rail 包括：
  - `BSP_PWR_SDCARD`
  - `BSP_PWR_CODEC_PA`
  - `BSP_PWR_AI_CHIP`
  - `BSP_PWR_GROVE`
- `BSP_PWR_BAT_ADC` 只在采样窗口短时打开，采样结束立即关闭，减少静态漏电。

### 2. 显示只在必要时工作
- 启动后先做低负载电池采样，再初始化界面。
- 背光默认值设为 `0`，避免出现“背光先亮、画面后到”的空白闪屏。
- 画面准备完成后再点亮背光，降低主观闪屏和无效耗电。
- 进入深睡前先清屏，再关背光，再关 LCD rail，避免最后一帧残留和睡前额外显示负载。

### 3. 电量估算在低负载窗口采样
- 启动采样发生在 LCD 仍关闭的阶段。
- 深睡前采样发生在背光关闭、LCD rail 关闭后的稳定窗口。
- 这样做的目的是减少“显示一亮，电压瞬间下探，再快速回升”的负载扰动。
- 待机损耗采用“深睡前采样”与“唤醒后采样”的差值估算，并换算为 `%/h`。

### 4. 深睡路径保持极短
- 交互只保留一个动作：按下滚轮按钮进入深度休眠。
- 唤醒后只恢复最小显示与采样逻辑，不恢复任何非必要外设。
- 不引入联网、音频、AI、文件系统写入等额外后台任务，避免隐藏功耗源。

### 5. 亮屏状态也避免无意义刷新
- 屏幕只显示最少信息：电量、当前电压、最近一次待机损耗、最近一次睡眠时长。
- 亮屏期间的电量刷新采用低频周期刷新，而不是高频连续采样，避免把“观测动作”本身变成额外耗电源。

## 后续扩展约束
- 如果后续要基于该固件实现其他功能，必须遵守以下顺序：
  1. 先在不改当前低功耗策略的情况下复测待机表现。
  2. 再只增加一个新功能点，例如联网、音频或传感器。
  3. 单独记录该功能增加前后的待机变化。
  4. 若待机显著变差，先回退定位，再决定是否保留该功能。
- 不允许一次性把多个高功耗模块同时接入，否则无法知道是哪一项破坏了待机基线。

## 推荐验证口径
- 冷启动后记录亮屏电量、电压。
- 按键进入深睡，保持固定待机时长。
- 唤醒后记录：
  - 深睡时长
  - 唤醒后电压
  - 损耗百分比
  - 换算后的 `%/h`
- 如需增加功能，应使用同一块板、同一电池状态、相近环境温度重复对比。

## 参考的官方文档段落

| 来源 | 参考段落 | 本基底如何使用 |
| --- | --- | --- |
| `watcher_devkit_baseline/01_device_fact_sheet.md` | `## 固化事实` | 对齐设备基础能力：`ESP32-S3`、`32MB Flash`、`8MB PSRAM`，避免项目配置偏离硬件基线。 |
| `watcher_devkit_baseline/03_bsp_minimum_contract.md` | `## 固化事实`、`### BSP 最小初始化顺序（必须）` | 只遵循最小必要上电与初始化顺序：先 I2C、再 IO Expander、再显示；不引入无关模块。 |
| `watcher_devkit_baseline/04_dependency_floor_and_fallback.md` | `### 可复现起跑线（必须先满足）` | 保持 `ESP-IDF v5.2.1`、`32MB Flash`、`8MB PSRAM OCT 80M` 这类起跑线配置一致。 |
| `watcher_reference/docs/hardware_modules/01_power_io_expander.md` | `## 固化事实`、`## 操作流程或约束` | 采用 IO 扩展器作为电源入口，遵守 `BSP_PWR_SYSTEM -> 延时 -> BSP_PWR_START_UP` 的上电时序。 |
| `watcher_reference/docs/hardware_modules/03_display_touch_lvgl.md` | `## 固化事实`、`## 操作流程或约束` | 显示只在 bus 与 rail 就绪后初始化；背光通过 `bsp_lcd_brightness_set` 单独控制。 |
| `watcher_reference/docs/hardware_modules/08_power_state_sleep.md` | `## 固化事实`、`## 操作流程或约束` | 深睡前主动拉低未使用 rail，并使用 IO expander 中断作为唤醒路径。 |
| `watcher_reference/docs/hardware_modules/09_rtc_battery_adc.md` | `## 固化事实`、`## 操作流程或约束` | 电池电压与百分比只作为估算值；本基底进一步用低负载采样降低误差。 |

## 与官方实现的边界
- 本文档引用的是硬件事实、初始化顺序与低功耗约束。
- 本基底没有迁移 `watcher_reference/use_cases/` 中的任务流、状态机、页面流程或业务调度结构。
- 后续开发若需要更多功能，也应继续沿用“只引用契约，不复用官方业务实现”的边界。

## 自检
1. 使用了哪些 RequiredSpec 来源：
   - `watcher_devkit_baseline/01_device_fact_sheet.md`
   - `watcher_devkit_baseline/03_bsp_minimum_contract.md`
   - `watcher_devkit_baseline/04_dependency_floor_and_fallback.md`
   - `watcher_reference/docs/hardware_modules/01_power_io_expander.md`
   - `watcher_reference/docs/hardware_modules/03_display_touch_lvgl.md`
   - `watcher_reference/docs/hardware_modules/08_power_state_sleep.md`
   - `watcher_reference/docs/hardware_modules/09_rtc_battery_adc.md`
2. 是否触发例外读取：
   - 未触发。
3. 为何可证明当前实现为自主设计：
   - 仅依据 BSP 公共接口、硬件时序与文档契约设计；未复用官方 use case 业务代码结构。
