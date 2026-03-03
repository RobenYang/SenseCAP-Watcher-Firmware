# sscma_client_ota

## 目标
演示 SSCMA 固件/模型 OTA 更新流程。

## 实现路径
- 用例目录: watcher_reference/use_cases/sscma_client_ota
- 构建入口: watcher_reference/use_cases/sscma_client_ota/CMakeLists.txt
- 组件依赖清单: watcher_reference/use_cases/sscma_client_ota/main/idf_component.yml

## 入口与资源
- 主要入口文件: watcher_reference/use_cases/sscma_client_ota/main/sscma_client_ota.c
- 资源目录: model/, spiffs/

## 依赖组件
- idf
- sensecap-watcher

## 最小构建方式
- cd watcher_reference/use_cases/sscma_client_ota
- idf.py set-target esp32s3
- idf.py build

## 与新固件解耦建议
可复用升级流程与分区访问方式，版本策略和发布流程按新项目重设。

## 证据来源
- watcher_reference/use_cases/sscma_client_ota/main/idf_component.yml
- watcher_reference/use_cases/sscma_client_ota/main/sscma_client_ota.c
- watcher_reference/use_cases/sscma_client_ota/CMakeLists.txt
