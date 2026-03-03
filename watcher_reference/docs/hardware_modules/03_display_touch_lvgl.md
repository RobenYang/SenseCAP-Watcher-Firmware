# 03 Display Touch LVGL

## 目标
沉淀显示、触摸、LVGL 初始化的必要顺序与时序要求。

## 适用范围
- LCD 与触摸面板初始化。
- LVGL 端口接入与输入设备绑定。

## 固化事实
| 项目 | 结论/要求 | 证据 |
| --- | --- | --- |
| LCD 上电控制 | 显示初始化前需先使能 BSP_PWR_LCD，并等待约 20ms | components/sensecap-watcher/sensecap-watcher.c |
| LVGL 初始化入口 | bsp_lvgl_init 与 bsp_lvgl_init_with_cfg 提供显示与输入一体初始化 | 同上 |
| 触摸初始化时序 | 触摸初始化后读一次数据，存在 50ms 与 100ms 延时 | 同上 |
| 背光控制 | 使用 LEDC，亮度由 bsp_lcd_brightness_set 控制 | 同上与头文件 |
| 分辨率参数 | DRV_LCD_H_RES=412, DRV_LCD_V_RES=412 | components/sensecap-watcher/include/sensecap-watcher.h |

## 操作流程或约束
1. 先保证 I2C/SPI/IO 扩展器可用，再进入显示初始化。
2. 不跳过 bsp_lcd_backlight_init 与 panel init 的官方流程。
3. 启用触摸或旋钮输入时，依赖 LVGL 输入设备配置选项。
4. 调试黑屏优先检查电源 rail、SPI3 引脚、背光 duty 与 panel init 返回值。

## 证据来源
- components/sensecap-watcher/sensecap-watcher.c
- components/sensecap-watcher/include/sensecap-watcher.h
- watcher_devkit_baseline/03_bsp_minimum_contract.md

## 变更记录
| 日期 | 变更项 | 基础版本 | 验证结果 | 风险 | 回退点 | 证据路径 |
| --- | --- | --- | --- | --- | --- | --- |
| 2026-03-02 | 初始化显示触摸 LVGL 调试文档 | FW 1.1.7 + ESP-IDF v5.2.1 | 已建立 | 中 | 回退官方 panel+touch 初始化链路 | components/sensecap-watcher/sensecap-watcher.c |