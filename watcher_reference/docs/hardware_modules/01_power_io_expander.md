# 01 Power And IO Expander

## 目标
固化电源域与 IO 扩展器初始化要求，避免上电顺序错误导致连锁故障。

## 适用范围
- 所有使用 sensecap-watcher BSP 的自定义固件。
- 所有涉及电源 rail 控制、扩展 IO 读写的场景。

## 固化事实
| 项目 | 结论/要求 | 证据 |
| --- | --- | --- |
| 电源入口 | bsp_io_expander_init 是电源域入口 | components/sensecap-watcher/sensecap-watcher.c |
| 上电顺序 | 先置 BSP_PWR_SYSTEM=1，再置 BSP_PWR_START_UP=1 | 同上 |
| 上电延时 | 顺序延时固定为 100ms + 50ms | 同上 |
| 冷启动默认策略 | BSP_PWR_START_UP 默认为 0，默认关闭可选 rail | components/sensecap-watcher/include/sensecap-watcher.h |
| 扩展 IO 模式 | DRV_IO_EXP_INPUT_MASK 设为输入，DRV_IO_EXP_OUTPUT_MASK 设为输出 | 同上与 C 文件实现 |

## 操作流程或约束
1. 先执行 bsp_i2c_bus_init，再执行 bsp_io_expander_init。
2. 未完成扩展器初始化前，不初始化显示、音频、AI 子系统。
3. 项目重构时保留官方上电时序，不建议删减固定延时。
4. 通过 bsp_exp_io_set_level 与 bsp_exp_io_get_level 统一访问扩展 IO。

## 证据来源
- components/sensecap-watcher/sensecap-watcher.c
- components/sensecap-watcher/include/sensecap-watcher.h
- watcher_devkit_baseline/03_bsp_minimum_contract.md

## 变更记录
| 日期 | 变更项 | 基础版本 | 验证结果 | 风险 | 回退点 | 证据路径 |
| --- | --- | --- | --- | --- | --- | --- |
| 2026-03-02 | 初始化电源与 IO 扩展器调试文档 | FW 1.1.7 + ESP-IDF v5.2.1 | 已建立 | 中 | 回到官方 BSP 上电顺序 | components/sensecap-watcher/* |