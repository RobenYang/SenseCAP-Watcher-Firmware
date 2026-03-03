# sscma_client_monitor

## 目标
演示 SSCMA 客户端监控与图像结果路径。

## 实现路径
- 用例目录: watcher_reference/use_cases/sscma_client_monitor
- 构建入口: watcher_reference/use_cases/sscma_client_monitor/CMakeLists.txt
- 组件依赖清单: watcher_reference/use_cases/sscma_client_monitor/main/idf_component.yml

## 入口与资源
- 主要入口文件: watcher_reference/use_cases/sscma_client_monitor/main/sscma_client_monitor.c
- 资源目录: 无额外资源目录

## 依赖组件
- idf
- sensecap-watcher
- esp_jpeg_simd

## 最小构建方式
- cd watcher_reference/use_cases/sscma_client_monitor
- idf.py set-target esp32s3
- idf.py build

## 与新固件解耦建议
保留 AI 数据通道初始化，监控展示与业务处理逻辑可拆分重构。

## 证据来源
- watcher_reference/use_cases/sscma_client_monitor/main/idf_component.yml
- watcher_reference/use_cases/sscma_client_monitor/main/sscma_client_monitor.c
- watcher_reference/use_cases/sscma_client_monitor/CMakeLists.txt
