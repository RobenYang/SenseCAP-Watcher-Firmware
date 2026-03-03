# 07 Input Knob Button RGB

## 目标
整理输入设备（旋钮/按键）与 RGB 灯控制的调试要点。

## 适用范围
- 旋钮交互、长按事件、状态指示灯。

## 固化事实
| 项目 | 结论/要求 | 证据 |
| --- | --- | --- |
| 旋钮引脚 | A=GPIO41, B=GPIO42 | components/sensecap-watcher/include/sensecap-watcher.h |
| 按键来源 | 旋钮按钮来自扩展 IO（IO_EXPANDER_PIN_NUM_3） | 同上 |
| 长按回调 | 通过 lvgl_port_encoder_btn_register_event_cb 绑定 | components/sensecap-watcher/sensecap-watcher.c |
| RGB 灯 | WS2812, 控制引脚 GPIO40 | include/sensecap-watcher.h 与 C 文件 |

## 操作流程或约束
1. 旋钮事件依赖 LVGL 输入设备初始化完成。
2. 按键异常先检查扩展 IO 初始化状态与 active level 配置。
3. RGB 显示异常先验证 bsp_rgb_init 是否已执行。
4. 输入层建议与业务事件分离，避免硬件中断逻辑耦合页面代码。

## 证据来源
- components/sensecap-watcher/sensecap-watcher.c
- components/sensecap-watcher/include/sensecap-watcher.h
- watcher_reference/use_cases/knob_rgb/main/knob_rgb.c

## 变更记录
| 日期 | 变更项 | 基础版本 | 验证结果 | 风险 | 回退点 | 证据路径 |
| --- | --- | --- | --- | --- | --- | --- |
| 2026-03-02 | 初始化输入与 RGB 调试文档 | FW 1.1.7 + ESP-IDF v5.2.1 | 已建立 | 低 | 回退官方输入设备配置 | components/sensecap-watcher/* |