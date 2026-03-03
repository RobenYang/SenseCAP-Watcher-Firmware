# 02 Bus And Pinmux

## 目标
汇总总线与关键引脚映射，减少 bring-up 阶段的硬件连线与初始化错误。

## 适用范围
- I2C、SPI、UART、I2S 外设初始化。
- 涉及多外设总线共享的调试场景。

## 固化事实
| 项目 | 结论/要求 | 证据 |
| --- | --- | --- |
| 通用 I2C | I2C0: SDA=GPIO47, SCL=GPIO48, 400kHz | components/sensecap-watcher/include/sensecap-watcher.h |
| 触摸 I2C | I2C1: SDA=GPIO39, SCL=GPIO38, 400kHz | 同上 |
| SPI2 | GPIO4/5/6，供 SD 与 SSCMA 链路使用 | 同上与 C 文件 |
| SPI3 QSPI | GPIO7/9/1/14/13，供 LCD 使用 | 同上 |
| Flasher UART | UART1 TX=GPIO17, RX=GPIO18, 921600 | 同上 |

## 操作流程或约束
1. 推荐总线初始化顺序：bsp_i2c_bus_init -> bsp_io_expander_init -> bsp_spi_bus_init -> bsp_uart_bus_init。
2. SPI2 为共享总线，初始化 AI 链路前必须先处理 SD CS 状态。
3. 调试引脚冲突时，优先核对 include/sensecap-watcher.h 中宏定义。

## 证据来源
- components/sensecap-watcher/include/sensecap-watcher.h
- components/sensecap-watcher/sensecap-watcher.c
- watcher_devkit_baseline/03_bsp_minimum_contract.md

## 变更记录
| 日期 | 变更项 | 基础版本 | 验证结果 | 风险 | 回退点 | 证据路径 |
| --- | --- | --- | --- | --- | --- | --- |
| 2026-03-02 | 初始化总线与引脚映射文档 | FW 1.1.7 + ESP-IDF v5.2.1 | 已建立 | 中 | 以 BSP 头文件宏定义为准 | components/sensecap-watcher/include/sensecap-watcher.h |