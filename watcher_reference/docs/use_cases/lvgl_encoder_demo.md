# lvgl_encoder_demo

## 目标
演示 LVGL 与编码器输入设备协同。

## 实现路径
- 用例目录: watcher_reference/use_cases/lvgl_encoder_demo
- 构建入口: watcher_reference/use_cases/lvgl_encoder_demo/CMakeLists.txt
- 组件依赖清单: watcher_reference/use_cases/lvgl_encoder_demo/main/idf_component.yml

## 入口与资源
- 主要入口文件: watcher_reference/use_cases/lvgl_encoder_demo/main/lvgl_encoder_demo.c
- 资源目录: 无额外资源目录

## 依赖组件
- idf
- sensecap-watcher

## 最小构建方式
- cd watcher_reference/use_cases/lvgl_encoder_demo
- idf.py set-target esp32s3
- idf.py build

## 与新固件解耦建议
保留输入设备绑定流程，业务交互逻辑与页面路由独立重写。

## 证据来源
- watcher_reference/use_cases/lvgl_encoder_demo/main/idf_component.yml
- watcher_reference/use_cases/lvgl_encoder_demo/main/lvgl_encoder_demo.c
- watcher_reference/use_cases/lvgl_encoder_demo/CMakeLists.txt
