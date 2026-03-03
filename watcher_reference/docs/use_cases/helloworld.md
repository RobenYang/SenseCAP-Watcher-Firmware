# helloworld

## 目标
最小板级 bring-up 示例，用于确认基础 BSP 可用。

## 实现路径
- 用例目录: watcher_reference/use_cases/helloworld
- 构建入口: watcher_reference/use_cases/helloworld/CMakeLists.txt
- 组件依赖清单: watcher_reference/use_cases/helloworld/main/idf_component.yml

## 入口与资源
- 主要入口文件: watcher_reference/use_cases/helloworld/main/helloworld.c
- 资源目录: 无额外资源目录

## 依赖组件
- idf
- sensecap-watcher

## 最小构建方式
- cd watcher_reference/use_cases/helloworld
- idf.py set-target esp32s3
- idf.py build

## 与新固件解耦建议
优先复用其启动顺序和最小验证路径，避免直接复制示例业务代码。

## 证据来源
- watcher_reference/use_cases/helloworld/main/idf_component.yml
- watcher_reference/use_cases/helloworld/main/helloworld.c
- watcher_reference/use_cases/helloworld/CMakeLists.txt
