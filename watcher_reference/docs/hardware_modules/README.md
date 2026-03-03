# 硬件模块调试须知索引

本目录按 BSP 模块整理硬件调试必要信息，重点是可直接复用的上电顺序、总线约束、初始化依赖与常见误区。

## 文档列表
- 01_power_io_expander.md
- 02_bus_and_pinmux.md
- 03_display_touch_lvgl.md
- 04_storage_spiffs_sd.md
- 05_audio_codec_i2s.md
- 06_ai_sscma_spi2.md
- 07_input_knob_button_rgb.md
- 08_power_state_sleep.md
- 09_rtc_battery_adc.md

## 通用原则
- 先保证硬件基础链路稳定，再接入业务功能。
- 优先复用官方已稳定的初始化顺序，不随意重排上电时序。
- 每次变更需保留证据路径，便于故障回溯。