# get_started

## 目标
最小化上手示例，展示基础初始化、UI显示与资源读取。

## 实现路径
- 用例目录: watcher_reference/use_cases/get_started
- 构建入口: watcher_reference/use_cases/get_started/CMakeLists.txt
- 组件依赖清单: watcher_reference/use_cases/get_started/main/idf_component.yml

## 入口与资源
- 主要入口文件: watcher_reference/use_cases/get_started/main/get_started.c
- 资源目录: spiffs/

## 依赖组件
- idf
- sensecap-watcher

## 最小构建方式
- cd watcher_reference/use_cases/get_started
- idf.py set-target esp32s3
- idf.py build

## 与新固件解耦建议
适合作为新固件最小骨架；业务功能从 app 层重新组织。

## 证据来源
- watcher_reference/use_cases/get_started/main/idf_component.yml
- watcher_reference/use_cases/get_started/main/get_started.c
- watcher_reference/use_cases/get_started/CMakeLists.txt
