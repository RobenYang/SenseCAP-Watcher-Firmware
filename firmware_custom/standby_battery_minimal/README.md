# 深睡唤醒极简录音固件

## 目标
- 作为 SenseCAP Watcher 自研低功耗录音基底。
- 设备处于深度休眠时，按下滚轮按钮后立即唤醒并尽快开始录音。
- 录音启动后尽快点亮极简界面，仅展示录音时长、当前状态、当前电量。
- 再次按下滚轮按钮后结束录音，完成保存后立即重新进入深度休眠。

## 当前行为
- 冷启动或刷机后，固件会直接进入深度休眠，等待滚轮按钮唤醒。
- 如果 USB-C 口有外部供电，则不再把供电变化误判为滚轮唤醒录音。
- 如果 USB-C 仅供电且没有数据连接，设备显示英文充电界面：电量、`Charging` / `Charged` 状态、预计充满时间。
- 如果 USB-C 供电且检测到数据连接，设备显示英文 `Debug Mode` 界面。
- 在 USB 供电界面下，按下滚轮按钮仍可正常进入录音。
- 从深睡唤醒后，优先顺序为：
  1. 初始化最小电源域与按键路径。
  2. 启动前面板录音指示灯闪烁。
  3. 启动音频采集并写入应用层缓冲。
  4. 挂载 SD 卡并创建录音文件与日志文件。
  5. 点亮低亮度极简界面。
- 录音期间：
  - 前面板 LED 以更低亮度的白光短脉冲闪烁提示，约每 2 秒闪 1 次。
  - 屏幕显示录音时长、状态、当前电量。
  - 电量只在本轮录音开始后做一次快速采样并显示，不做周期刷新。
- 停止录音后：
  - 界面切换为 `Saving...`。
  - 录音数据排空并落盘到 SD 卡。
  - 若 USB-C 仍在供电，则录音完成后切回 USB 供电界面。
  - 若 USB-C 未供电，则关闭显示、停止 LED、采集一次睡前稳定电量并写日志，然后进入深度休眠。

## 文件输出
- 录音文件保存在 SD 卡根目录：`/sdcard/r00000.pcm`、`/sdcard/r00001.pcm` …
- 日志文件固定为：`/sdcard/events.csv`
- 录音格式为裸 `PCM`：`16kHz / 16bit / mono`

## 日志策略
- 仅记录关键动作，不做高频写盘。
- 当前事件包括：
  - `WAKE`
  - `REC_START`
  - `REC_STOP_REQ`
  - `REC_SAVING`
  - `REC_SAVED`
  - `PRE_SLEEP_SAMPLE`
  - `SLEEP_ENTER`
  - `ERROR`
- 日志字段固定为：
  - `seq,rtc_us,uptime_ms,event,wakeup_cause,file_name,bytes_written,duration_ms,battery_mv,battery_percent,error_code`

## 运行前准备
- 录音文件保存在 SD 卡上，首次测试前请先手动清空 SD 卡。
- 当前实现不包含“自动清空 SD 卡”逻辑，避免误删。

## USB-C 行为说明
- 供电状态只在以下场景主动检查：
  - 开机/唤醒后的启动分流。
  - 录音进行中的低频轮询，用于判断是否在录音期间插入 USB-C。
  - USB 供电界面刷新。
- 非 USB 供电状态下，不会为检查 USB 而额外引入高频轮询。
- `Debug Mode` 判定基于 ESP32-S3 的 `USB Serial/JTAG` 连接状态。
- `Charging` 预计时间为**线性粗略估算**，当前默认按“每 1% 电量约 2 分钟”显示，仅用于提示，不作为精确充电时间承诺。

## 构建
```powershell
cd firmware_custom/standby_battery_minimal
idf.py set-target esp32s3
idf.py build
```

## Dependency Notes
- Some installable dependencies are intentionally not tracked in Git to keep the repo lightweight.
- `components/lvgl/` is treated as a locally rehydrated dependency and should not be committed.
- On a new machine, run `idf.py reconfigure` or `idf.py build` in `firmware_custom/standby_battery_minimal/` to restore managed dependencies into local working directories such as `managed_components/`.
- Keep `dependencies.lock` so the restored dependency set stays reproducible across devices.

## 烧录与观察
- 烧录后设备会进入深度休眠。
- 按下滚轮按钮后开始唤醒并录音。
- 如果插入 USB-C 电源：
  - 纯供电：显示 `Charging` / `Charged` 界面。
  - 供电 + 数据：显示 `Debug Mode` 界面。
- 录音开始后屏幕会显示：
  - 顶部：当前电量
  - 中央：录音时长
  - 底部：当前状态
- 再次按下滚轮按钮后停止录音：
  - 若未插 USB-C，则回到深度休眠。
  - 若已插 USB-C，则回到 USB 供电界面。

## 约束
- 不复用 `watcher_reference/use_cases/` 的任务流、状态机或页面结构。
- 录音导出、回放、波形、文件浏览暂不在本版本范围内。
- 当前版本重点是“深睡唤醒录音时延 + 关键日志 + 最小界面”。
