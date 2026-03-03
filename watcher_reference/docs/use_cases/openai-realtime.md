# openai-realtime

## 目标
演示实时语音链路（网络、音频、会话）与设备端集成。

## 实现路径
- 用例目录: watcher_reference/use_cases/openai-realtime
- 构建入口: watcher_reference/use_cases/openai-realtime/CMakeLists.txt
- 组件依赖清单: watcher_reference/use_cases/openai-realtime/src/idf_component.yml

## 入口与资源
- 主要入口文件: watcher_reference/use_cases/openai-realtime/src/main.cpp
- 资源目录: components/, deps/, firmware/

## 依赖组件
- idf
- espressif/es8311
- sensecap-watcher
- esp_io_expander_pca95xx_16bit
- esp_lvgl_port
- sscma_client

## 最小构建方式
- cd watcher_reference/use_cases/openai-realtime
- idf.py set-target esp32s3
- idf.py build

## 与新固件解耦建议
可复用音频与网络接入范式，具体协议与会话控制建议独立设计。

## 证据来源
- watcher_reference/use_cases/openai-realtime/src/idf_component.yml
- watcher_reference/use_cases/openai-realtime/src/main.cpp
- watcher_reference/use_cases/openai-realtime/CMakeLists.txt
