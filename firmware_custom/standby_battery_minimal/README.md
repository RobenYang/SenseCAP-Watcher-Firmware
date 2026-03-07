# 极简待机续航测试固件

## 目标
- 仅用于测试设备待机续航。
- 不运行 Wi‑Fi、蓝牙、音频、NPU、SD 卡、Grove 等额外功能。
- 亮屏时只显示当前电量与最近一次待机估算值；按下滚轮按钮后进入深度休眠。

## 设计原则
- 开发目录固定在 `firmware_custom/standby_battery_minimal/`。
- 不复用 `watcher_reference/use_cases/` 的任务流、状态机或页面结构。
- 未使用的 rail 默认保持关闭：`BSP_PWR_SDCARD`、`BSP_PWR_CODEC_PA`、`BSP_PWR_AI_CHIP`、`BSP_PWR_GROVE`。
- 电池 ADC rail 只在采样瞬间打开，采样完成立即关闭，减少静态漏电。

## 电量读数抖动的处理
- 启动后先在 **LCD 仍关闭** 的低负载状态下采样电池，再初始化显示。
- 进入深睡前先关闭背光和 LCD rail，等待负载回落后再采样一次。
- 待机损耗使用“深睡前低负载采样”与“唤醒后低负载采样”做比较，尽量避免你之前观察到的“先掉很低再快速回升”对结果的污染。

## 构建
```powershell
cd firmware_custom/standby_battery_minimal
idf.py set-target esp32s3
idf.py build
```

## 运行
- 烧录后，默认亮屏显示电量。
- 按下滚轮按钮，设备关闭显示并进入深度休眠。
- 再按滚轮按钮，设备从深度休眠唤醒，并显示最近一次待机估算值（`%/h`）。
