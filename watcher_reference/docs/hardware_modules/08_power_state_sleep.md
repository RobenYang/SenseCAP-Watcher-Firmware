# 08 Power State And Sleep

## 目标
明确关机、深睡与充电状态检测机制，避免误关电与异常唤醒。

## 适用范围
- 低功耗策略、待机策略、关机行为。

## 固化事实
| 项目 | 结论/要求 | 证据 |
| --- | --- | --- |
| 深睡前拉低 rail | 深睡前会关闭 SDCARD/CODEC_PA/GROVE/BAT_ADC/LCD/AI_CHIP | components/sensecap-watcher/sensecap-watcher.c |
| 深睡唤醒源 | 使用 IO_EXPANDER_INT 作为 ext0 唤醒源 | 同上 |
| 关机动作 | bsp_system_shutdown 直接拉低 BSP_PWR_SYSTEM | 同上 |
| 充电状态检测 | bsp_system_is_charging 与 standby/battery present 都基于扩展 IO 电平 | 同上与头文件 |

## 操作流程或约束
1. 进入深睡前先确认关键业务状态已保存。
2. 关机后不应再访问依赖系统 rail 的外设。
3. 若出现异常唤醒，优先检查 IO_EXPANDER_INT 电平与外部电路抖动。
4. 低功耗调试需结合电源域控制与任务停止顺序一起验证。

## 证据来源
- components/sensecap-watcher/sensecap-watcher.c
- components/sensecap-watcher/include/sensecap-watcher.h
- watcher_reference/use_cases/battery_standby_test/main/battery_standby_test.c

## 变更记录
| 日期 | 变更项 | 基础版本 | 验证结果 | 风险 | 回退点 | 证据路径 |
| --- | --- | --- | --- | --- | --- | --- |
| 2026-03-02 | 初始化电源状态与睡眠调试文档 | FW 1.1.7 + ESP-IDF v5.2.1 | 已建立 | 中 | 回退官方休眠与关机接口 | components/sensecap-watcher/sensecap-watcher.c |