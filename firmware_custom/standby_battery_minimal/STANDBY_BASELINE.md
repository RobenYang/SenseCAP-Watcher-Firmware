# 深睡录音基底说明

## 目标
- 固化当前版本为何是“深睡唤醒录音”基底，而不是待机功耗统计工具。
- 在保证深睡路径短的前提下，为后续录音相关功能提供稳定起点。
- 把功耗分析从“机内实时计算”改为“关键事件落日志，后续离线分析”。

## 基底定位
- 本固件是 **SenseCAP Watcher 自研深睡录音基底**。
- 这一版优先保证四件事：
  1. 深睡到开始录音的路径尽量短。
  2. 录音状态有最小但明确的屏幕与 LED 反馈。
  3. 录音文件与关键事件日志可靠落到 SD 卡。
  4. 睡前再采一次稳定电量，为后续功耗分析提供原始事实。

## 当前结论
- 机内不再计算 `%/h` 或睡眠期损耗结论。
- 本版只负责记录原始事实：唤醒、录音开始、停止保存、睡前稳定电量等关键节点。
- 后续功耗分析应由日志读取阶段完成，而不是在设备内实时计算。

## 设计策略

### 1. 唤醒优先录音
- 从深睡唤醒后，优先启动录音相关链路。
- 屏幕初始化放在录音启动之后，避免界面优先拖慢录音起点。
- 唤醒初期不做稳定电量采样，避免额外等待。
- 但如果唤醒原因是 USB-C 供电变化，而不是滚轮按钮，则优先进入 USB 供电界面，而不是直接开始录音。

### 2. 极简界面
- 保留 LCD，但只显示：
  - 当前电量
  - 录音时长
  - 当前状态
- USB-C 供电时额外允许显示：
  - `Charging`
  - `Charged`
  - `Debug Mode`
  - `ETA HH:MM`（粗略线性估算）
- 不显示波形、菜单、文件名、待机损耗结论等额外信息。
- 屏幕亮度固定低值，减少录音期间显示功耗。

### 3. 稀疏日志
- 日志只在关键动作时写入 SD，不做周期刷盘。
- 日志目的是给后续离线调试提供时间点与电量事实，而不是在设备内得出分析结论。
- 睡前稳定电量只记录，不在设备内与历史样本相减推导。

### 4. 深睡前最小收尾
- 停止录音后先完成文件排空与保存。
- 若 USB-C 未供电，则之后关闭 LED、关闭显示、采集睡前稳定电量，并立刻回到深度休眠。
- 若 USB-C 已供电，则录音结束后切回 USB 供电界面，不直接回睡。

### 5. USB-C 供电分流
- `VBUS` 状态只在启动分流、USB 供电界面和录音期间低频检测，不在普通未供电待机场景引入额外高频检查。
- 纯供电影响设备显示充电界面；供电加数据连接时显示 `Debug Mode`。
- `Debug Mode` 判定使用 ESP32-S3 的 `USB Serial/JTAG` 连接状态。
- 为了区分“滚轮唤醒”和“USB-C 插入唤醒”，固件会在 RTC 中记住上次睡前的 `VBUS` 状态，并在下次启动时用于判断唤醒来源。
- 同样会记录上次睡前的 SD 卡插入状态；如果唤醒后判定只是 SD 检测脚变化，则直接回睡，不进入录音状态机。

## 当前低功耗原则
- 未使用 rail 默认保持关闭：
  - `BSP_PWR_SDCARD`
  - `BSP_PWR_CODEC_PA`
  - `BSP_PWR_AI_CHIP`
  - `BSP_PWR_GROVE`
- `BSP_PWR_SDCARD` 只在录音与日志写盘阶段打开。
- `BSP_PWR_BAT_ADC` 只在快速电量显示或睡前稳定采样窗口短时打开。
- 深睡前关闭 LCD 与非必要 rail，保持下一次唤醒路径可重复。

## 推荐验证口径
- 每轮验证至少关注以下结果：
  - `WAKE -> REC_START` 串口时间顺序
  - LED 是否先于屏幕进入录音提示状态
  - 屏幕是否正确显示 `Recording` 与录音时长
  - `Saving...` 后是否成功生成 `.pcm` 文件
  - `events.csv` 是否追加关键事件与睡前稳定电量
  - 插入 USB-C 电源后不应自动开始录音
  - 纯供电时应显示 `Charging` / `Charged`
  - 供电加数据时应显示 `Debug Mode`
  - 录音中插入 USB-C 后，本轮录音结束再切回 USB 界面
- 如果后续继续扩展功能，应继续保持“每次只增加一个变量，再复测”的方式。

## 参考的官方文档段落

| 来源 | 参考段落 | 本基底如何使用 |
| --- | --- | --- |
| `watcher_devkit_baseline/03_bsp_minimum_contract.md` | `## 固化事实`、`### BSP 最小初始化顺序（必须）` | 对齐最小 bring-up 顺序，保留 I2C、IO Expander、显示、音频、SD 的初始化约束。 |
| `watcher_reference/docs/hardware_modules/04_storage_spiffs_sd.md` | `## 固化事实`、`## 操作流程或约束` | 只使用 SD 默认挂载路径与存储约束，录音与日志都写到 SD。 |
| `watcher_reference/docs/hardware_modules/05_audio_codec_i2s.md` | `## 固化事实`、`## 操作流程或约束` | 采用既有 I2S 参数与 codec 入口，保持 16k/16bit/mono。 |
| `watcher_reference/docs/hardware_modules/07_input_knob_button_rgb.md` | `## 固化事实`、`## 操作流程或约束` | 使用滚轮按钮作为唤醒/停止录音输入，使用板载 RGB 灯作为录音指示。 |
| `watcher_reference/docs/hardware_modules/08_power_state_sleep.md` | `## 固化事实`、`## 操作流程或约束` | 延续 IO expander 中断唤醒与深睡前 rail 下电策略。 |

## 与官方实现的边界
- 本基底只引用 BSP 接口、硬件引脚、总线与深睡约束。
- 没有迁移 `watcher_reference/use_cases/` 中的任务流、页面流转、状态机或录音业务结构。
- 当前录音状态机、日志格式、界面布局和文件落盘流程均为本目录内自研设计。

## 自检
1. 使用了哪些 RequiredSpec 来源：
   - `watcher_devkit_baseline/03_bsp_minimum_contract.md`
   - `watcher_reference/docs/hardware_modules/04_storage_spiffs_sd.md`
   - `watcher_reference/docs/hardware_modules/05_audio_codec_i2s.md`
   - `watcher_reference/docs/hardware_modules/07_input_knob_button_rgb.md`
   - `watcher_reference/docs/hardware_modules/08_power_state_sleep.md`
   - `components/sensecap-watcher/include/sensecap-watcher.h`
2. 是否触发例外读取：
   - 未触发。
3. 为何可证明当前实现为自主设计：
   - 仅依据硬件契约、BSP 接口和低功耗约束完成自研录音状态机；未复用官方 use case 业务实现。
