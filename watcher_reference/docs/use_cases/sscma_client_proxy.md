# sscma_client_proxy

## 目标
演示 SSCMA 代理通信链路与透传处理。

## 实现路径
- 用例目录: watcher_reference/use_cases/sscma_client_proxy
- 构建入口: watcher_reference/use_cases/sscma_client_proxy/CMakeLists.txt
- 组件依赖清单: watcher_reference/use_cases/sscma_client_proxy/main/idf_component.yml

## 入口与资源
- 主要入口文件: watcher_reference/use_cases/sscma_client_proxy/main/sscma_client_proxy.c
- 资源目录: 无额外资源目录

## 依赖组件
- idf
- sensecap-watcher
- esp_jpeg_simd

## 最小构建方式
- cd watcher_reference/use_cases/sscma_client_proxy
- idf.py set-target esp32s3
- idf.py build

## 与新固件解耦建议
保留通信总线初始化，代理协议与上层控制协议建议独立定义。

## 证据来源
- watcher_reference/use_cases/sscma_client_proxy/main/idf_component.yml
- watcher_reference/use_cases/sscma_client_proxy/main/sscma_client_proxy.c
- watcher_reference/use_cases/sscma_client_proxy/CMakeLists.txt
