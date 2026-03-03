# 09 RTC Battery ADC

## 目标
整理 RTC 与电池 ADC 路径，作为系统时间与电量相关功能的调试基线。

## 适用范围
- RTC 时间读写、定时唤醒。
- 电池电压采样与电量估算。

## 固化事实
| 项目 | 结论/要求 | 证据 |
| --- | --- | --- |
| RTC 器件地址 | PCF8563 I2C 地址为 0x51 | components/sensecap-watcher/include/sensecap-watcher.h |
| RTC 接口 | bsp_rtc_init/get_time/set_time/set_timer | 同上与 C 文件 |
| 电池 ADC 通道 | ADC_CHANNEL_2（GPIO3），BSP_BAT_VOL_RATIO 已定义 | include/sensecap-watcher.h |
| 电压与电量接口 | bsp_battery_get_voltage 与 bsp_battery_get_percent | sensecap-watcher.c |
| 电池在位检测 | bsp_battery_is_present 基于扩展 IO 检测 | 同上 |

## 操作流程或约束
1. 读取 RTC/电池前先确认通用 I2C 与扩展 IO 已初始化。
2. 电池百分比仅作为估算值，关键策略应结合实测校准。
3. 若时间异常，先检查 RTC 寄存器读写链路与 I2C 探测结果。
4. 若电量异常，优先排查 ADC 标定与分压比是否与硬件一致。

## 证据来源
- components/sensecap-watcher/include/sensecap-watcher.h
- components/sensecap-watcher/sensecap-watcher.c
- watcher_devkit_baseline/01_device_fact_sheet.md

## 变更记录
| 日期 | 变更项 | 基础版本 | 验证结果 | 风险 | 回退点 | 证据路径 |
| --- | --- | --- | --- | --- | --- | --- |
| 2026-03-02 | 初始化 RTC 与电池 ADC 调试文档 | FW 1.1.7 + ESP-IDF v5.2.1 | 已建立 | 低 | 回退官方采样与 RTC 接口 | components/sensecap-watcher/* |