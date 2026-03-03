# factory_firmware

## 目标
官方全功能固件示例，覆盖任务流、UI、音频、AI、联网等完整链路。

## 实现路径
- 用例目录: watcher_reference/use_cases/factory_firmware
- 构建入口: watcher_reference/use_cases/factory_firmware/CMakeLists.txt
- 组件依赖清单: watcher_reference/use_cases/factory_firmware/main/idf_component.yml

## 入口与资源
- 主要入口文件: watcher_reference/use_cases/factory_firmware/main/main.c
- 资源目录: docs/, spiffs/

## 依赖组件
- idf
- sscma_client
- sensecap-watcher
- esp_io_expander_pca95xx_16bit
- espressif/esp-sr
- chmorgan/esp-audio-player
- chmorgan/esp-file-iterator
- esp_jpeg_simd
- iperf

## 最小构建方式
- cd watcher_reference/use_cases/factory_firmware
- idf.py set-target esp32s3
- idf.py build

## 与新固件解耦建议
建议只保留可复用 BSP 初始化与硬件配置，不直接继承任务流业务逻辑。

## 证据来源
- watcher_reference/use_cases/factory_firmware/main/idf_component.yml
- watcher_reference/use_cases/factory_firmware/main/main.c
- watcher_reference/use_cases/factory_firmware/CMakeLists.txt
