# rlottie_example

## 目标
演示 RLottie 动画资源加载与显示。

## 实现路径
- 用例目录: watcher_reference/use_cases/rlottie_example
- 构建入口: watcher_reference/use_cases/rlottie_example/CMakeLists.txt
- 组件依赖清单: watcher_reference/use_cases/rlottie_example/main/idf_component.yml

## 入口与资源
- 主要入口文件: watcher_reference/use_cases/rlottie_example/main/rlottie_example.c
- 资源目录: spiffs/

## 依赖组件
- idf
- sensecap-watcher

## 最小构建方式
- cd watcher_reference/use_cases/rlottie_example
- idf.py set-target esp32s3
- idf.py build

## 与新固件解耦建议
仅复用动画渲染管线与资源管理，UI 状态机按新产品逻辑重写。

## 证据来源
- watcher_reference/use_cases/rlottie_example/main/idf_component.yml
- watcher_reference/use_cases/rlottie_example/main/rlottie_example.c
- watcher_reference/use_cases/rlottie_example/CMakeLists.txt
