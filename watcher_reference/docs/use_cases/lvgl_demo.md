# lvgl_demo

## 目标
演示 LVGL 显示渲染与基础控件使用。

## 实现路径
- 用例目录: watcher_reference/use_cases/lvgl_demo
- 构建入口: watcher_reference/use_cases/lvgl_demo/CMakeLists.txt
- 组件依赖清单: watcher_reference/use_cases/lvgl_demo/main/idf_component.yml

## 入口与资源
- 主要入口文件: watcher_reference/use_cases/lvgl_demo/main/lvgl_demo.c
- 资源目录: 无额外资源目录

## 依赖组件
- idf
- sensecap-watcher

## 最小构建方式
- cd watcher_reference/use_cases/lvgl_demo
- idf.py set-target esp32s3
- idf.py build

## 与新固件解耦建议
可复用显示初始化与 LVGL 端口配置，UI 页面结构建议重建。

## 证据来源
- watcher_reference/use_cases/lvgl_demo/main/idf_component.yml
- watcher_reference/use_cases/lvgl_demo/main/lvgl_demo.c
- watcher_reference/use_cases/lvgl_demo/CMakeLists.txt
